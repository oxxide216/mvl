#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "../libs/mvm/misc.h"
#include "../libs/lexgen-runtime/runtime.h"
#include "../grammar.h"
#include "shl_log.h"
#include "ir.h"
#include "shl_arena.h"

typedef enum {
  BlockKindProc = 0,
  BlockKindIf,
  BlockKindWhile,
} BlockKind;

typedef struct {
  BlockKind kind;
  Str       end_label_name;
} Block;

typedef Da(Block) Blocks;

typedef struct {
  Tokens tokens;
  u32    index;
  Blocks blocks;
  u32    max_labels_count;
} Parser;

static Str token_id_names[] = {
  STR_LIT("new line"),
  STR_LIT("whitespace"),
  STR_LIT("comment"),
  STR_LIT("string literal"),
  STR_LIT("character literal"),
  STR_LIT("`proc`"),
  STR_LIT("`if`"),
  STR_LIT("`elif`"),
  STR_LIT("`else`"),
  STR_LIT("`while`"),
  STR_LIT("`end`"),
  STR_LIT("`break`"),
  STR_LIT("`continue`"),
  STR_LIT("`ret`"),
  STR_LIT("`retval`"),
  STR_LIT("`include`"),
  STR_LIT("`static`"),
  STR_LIT("`asm`"),
  STR_LIT("`naked`"),
  STR_LIT("`cast`"),
  STR_LIT("`macro`"),
  STR_LIT("`group`"),
  STR_LIT("`record`"),
  STR_LIT("identifier"),
  STR_LIT("number"),
  STR_LIT("`(`"),
  STR_LIT("`)`"),
  STR_LIT("`[`"),
  STR_LIT("`]`"),
  STR_LIT("`,`"),
  STR_LIT("`:`"),
  STR_LIT("`==`"),
  STR_LIT("`!=`"),
  STR_LIT("`>=`"),
  STR_LIT("`<=`"),
  STR_LIT("`>`"),
  STR_LIT("`<`"),
  STR_LIT("`=`"),
  STR_LIT("`->`"),
  STR_LIT("'&'"),
  STR_LIT("`*`"),
  STR_LIT("`!`"),
  STR_LIT("`$`"),
};

static u64 expected_begin_token_ids = MASK(TT_PROC) | MASK(TT_IF) |
                                      MASK(TT_ELSE) | MASK(TT_WHILE) |
                                      MASK(TT_END) | MASK(TT_BREAK) |
                                      MASK(TT_CONTINUE) | MASK(TT_RET) |
                                      MASK(TT_RETVAL) | MASK(TT_IDENT) |
                                      MASK(TT_INCLUDE) | MASK(TT_STATIC) |
                                      MASK(TT_ASM) | MASK(TT_DEREF) |
                                      MASK(TT_MACRO) | MASK(TT_MACRO_CALL) |
                                      MASK(TT_GROUP) | MASK(TT_RECORD);

static void print_id_mask(u64 id_mask, Str lexeme, FILE *stream) {
  u32 len = ARRAY_LEN(token_id_names);
  u32 max_matched_ids_count = 0;

  for (u32 i = 0; i < len; ++i)
    if (id_mask & MASK(i))
      ++max_matched_ids_count;

  u32 matched_ids_count = 0;
  for (u32 i = 0; i < len; ++i) {
    if (!(id_mask & MASK(i)))
      continue;

    if (max_matched_ids_count > 1) {
      if (matched_ids_count + 1 == max_matched_ids_count)
        fputs(" or ", stream);
      else if (matched_ids_count > 0)
        fputs(", ", stream);
    }

    if ((i == TT_NUMBER || i == TT_IDENT) && lexeme.len != 0) {
      fputc('`', stream);
      str_fprint(stream, lexeme);
      fputc('`', stream);
    } else {
      str_fprint(stream, token_id_names[i]);
    }

    ++matched_ids_count;
  }
}

void expect_token(Token *token, u64 id_mask) {
  if (!token) {
    ERROR("Expected ");
    print_id_mask(id_mask, (Str) {0}, stderr);
    fputs(", but got EOF\n", stderr);
    exit(1);
  }

  if (token && id_mask & ((u64) 1 << token->id))
    return;

  PERROR(STR_FMT":%u:%u: ", "Expected ",
         STR_ARG(token->file_path),
         token->row + 1, token->col + 1);
  print_id_mask(id_mask, (Str) {0}, stderr);
  fputs(", but got ", stderr);
  print_id_mask(MASK(token->id), token->lexeme, stderr);
  fputc('\n', stderr);

  exit(1);
}

static Token *parser_peek_token(Parser *parser, u32 offset) {
  if (parser->index + offset >= parser->tokens.len)
    return NULL;

  return parser->tokens.items + parser->index + offset;
}

static Token *parser_next_token(Parser *parser) {
  Token *token = parser_peek_token(parser, 0);
  ++parser->index;
  return token;
}

static Token *parser_expect_token(Parser *parser, u64 id_mask) {
  Token *token = parser_next_token(parser);
  expect_token(token, id_mask);
  return token;
}

static ArgKind token_id_to_arg_kind(u64 id) {
  switch (id) {
  case TT_NUMBER:       return ArgKindValue;
  case TT_IDENT:        return ArgKindVar;

  default: {
    ERROR("Wrong token id\n");
    exit(1);
  }
  }
}

static TypeKind str_to_type_kind(Str str) {
  if (str_eq(str, STR_LIT("unit")))
    return TypeKindUnit;

  if (str_eq(str, STR_LIT("s64")))
    return TypeKindS64;

  if (str_eq(str, STR_LIT("s32")))
    return TypeKindS32;

  if (str_eq(str, STR_LIT("s16")))
    return TypeKindS16;

  if (str_eq(str, STR_LIT("s8")))
    return TypeKindS8;

  if (str_eq(str, STR_LIT("u64")))
    return TypeKindU64;

  if (str_eq(str, STR_LIT("u32")))
    return TypeKindU32;

  if (str_eq(str, STR_LIT("u16")))
    return TypeKindU16;

  if (str_eq(str, STR_LIT("u8")))
    return TypeKindU8;

  // Record pointer type
  return TypeKindPtr;
}

static IrArgValue str_to_number_ir_arg_value(Str str) {
  i64 number = 0;
  bool is_neg = false;
  u32 i = 0;

  if (str.len > 0 && str.ptr[0] == '-') {
    is_neg = true;
    ++i;
  }

  while (i < (u32) str.len && isdigit(str.ptr[i])) {
    number *= 10;
    number += str.ptr[i] - '0';
    ++i;
  }

  if (is_neg)
    number *= -1;

  Type *type = aalloc(sizeof(Type));

  if (i < (u32) str.len) {
    str.ptr += i;
    str.len -= i;

    TypeKind kind = str_to_type_kind(str);
    type->kind = kind;

    switch (type->kind) {
    case TypeKindS64: return (IrArgValue) { type, { ._s64 = number } };
    case TypeKindS32: return (IrArgValue) { type, { ._s32 = number } };
    case TypeKindS16: return (IrArgValue) { type, { ._s16 = number } };
    case TypeKindS8:  return (IrArgValue) { type, { ._s8 = number } };
    case TypeKindU64: return (IrArgValue) { type, { ._u64 = number } };
    case TypeKindU32: return (IrArgValue) { type, { ._u32 = number } };
    case TypeKindU16: return (IrArgValue) { type, { ._u16 = number } };
    case TypeKindU8:  return (IrArgValue) { type, { ._u8 = number } };

    default: {
      ERROR("Unknown type name: "STR_FMT"\n", STR_ARG(str));
      exit(1);
    }
    }
  }

  type->kind = TypeKindS64;
  return (IrArgValue) { type, { ._s64 = number } };
}

static IrArg str_to_ir_arg(Str text, IrArgKind kind) {
  IrArg ir_arg;
  ir_arg.kind = kind;

  switch (kind) {
  case IrArgKindValue: {
    ir_arg.as.value = str_to_number_ir_arg_value(text);
  } break;

  case IrArgKindVar: {
    ir_arg.as.var = text;
  } break;

  default: {
    ERROR("Wrong argument kind\n");
    exit(1);
  }
  }

  return ir_arg;
}

static Type *parser_parse_type(Parser *parser) {
  Type *type = aalloc(sizeof(Type));
  *type = (Type) {0};

  Token *token = parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_REF));

  if (token->id == TT_IDENT) {
    type->kind = str_to_type_kind(token->lexeme);
    type->ptr_target = NULL;
  } else if (token->id == TT_REF) {
    type->kind = TypeKindPtr;
    type->ptr_target = parser_parse_type(parser);
  }

  return type;
}

static IrArg parser_parse_arg(Parser *parser) {
  Token *token = parser_expect_token(parser, MASK(TT_NUMBER) | MASK(TT_IDENT));

  IrArgKind arg_kind;
  if (token->id == TT_NUMBER)
    arg_kind = IrArgKindValue;
  else if (token->id == TT_IDENT)
    arg_kind = IrArgKindVar;

  return str_to_ir_arg(token->lexeme, arg_kind);
}

static IrProc parser_parse_proc_def(Parser *parser) {
  IrProc proc = {0};

  Token *token = parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_NAKED));

  if (token->id == TT_NAKED) {
    proc.is_naked = true;
    token = parser_expect_token(parser, MASK(TT_IDENT));
  }

  proc.name = token->lexeme;

  parser_expect_token(parser, MASK(TT_OPAREN));

  token = parser_peek_token(parser, 0);
  while (token && token->id != TT_CPAREN) {
    Token *param_name_token = parser_expect_token(parser, MASK(TT_IDENT));
    parser_expect_token(parser, MASK(TT_COLON));
    Type *param_type = parser_parse_type(parser);
    IrProcParam param = { param_name_token->lexeme, param_type };
    DA_APPEND(proc.params, param);

    token = parser_expect_token(parser, MASK(TT_COMMA) |
                                        MASK(TT_CPAREN));

    if (token->id == TT_COMMA) {
      token = parser_peek_token(parser, 0);
      if (token->id == TT_CPAREN)
        break;
    }
  }

  parser_expect_token(parser, MASK(TT_CPAREN));

  token = parser_peek_token(parser, 0);
  if (token->id == TT_RIGHT_ARROW) {
    parser_next_token(parser);
    proc.ret_val_type = parser_parse_type(parser);
  }

  parser_expect_token(parser, MASK(TT_COLON));

  return proc;
}

RelOp parser_parse_rel_op(Parser *parser) {
  Token *token = parser_expect_token(parser, MASK(TT_EQ) | MASK(TT_NE) |
                                             MASK(TT_GE) | MASK(TT_LE) |
                                             MASK(TT_GT) | MASK(TT_LS));

  switch (token->id) {
  case TT_EQ: return RelOpNotEqual;
  case TT_NE: return RelOpEqual;
  case TT_GE: return RelOpLess;
  case TT_LE: return RelOpGreater;
  case TT_GT: return RelOpLessOrEqual;
  case TT_LS: return RelOpGreaterOrEqual;
  default:    return 0;
  }
}

Str gen_label_name(u32 index) {
  StringBuilder sb = {0};
  sb_push(&sb, "label");
  sb_push_u32(&sb, index);
  return sb_to_str(sb);
}

IrProcs parse(Tokens tokens) {
  Parser parser = {0};
  parser.tokens = tokens;
  IrProcs ir = {0};
  IrProc *last_proc = NULL;
  u32 recursion_level = 0;

  Token *token = parser_expect_token(&parser, expected_begin_token_ids);
  while (token) {
    if (!last_proc && token->id != TT_PROC) {
      ERROR("Instruction can be only inside of a procedure\n");
      exit(1);
    }

    switch (token->id) {
    case TT_PROC: {
      IrProc proc = parser_parse_proc_def(&parser);
      DA_APPEND(ir, proc);

      last_proc = ir.items + ir.len - 1;
      recursion_level = 0;

      Block new_block = { BlockKindProc, {0} };
      DA_APPEND(parser.blocks, new_block);
    } break;

    case TT_IF:
    case TT_WHILE: {
      IrArg arg0 = parser_parse_arg(&parser);
      RelOp rel_op = parser_parse_rel_op(&parser);
      IrArg arg1 = parser_parse_arg(&parser);

      parser_expect_token(&parser, MASK(TT_COLON));

      ++recursion_level;

      Str end_label_name = gen_label_name(parser.max_labels_count++);
      Block new_block;
      IrInstr instr;


      if (token->id == TT_IF) {
        new_block = (Block) { BlockKindIf, end_label_name };
        instr = (IrInstr) { IrInstrKindIf, { ._if = { arg0, arg1, rel_op, end_label_name } } };
      } else {
        new_block = (Block) { BlockKindWhile, end_label_name };
        instr = (IrInstr) { IrInstrKindWhile, { ._while = { arg0, arg1, rel_op, end_label_name } } };
      }

      DA_APPEND(parser.blocks, new_block);
      DA_APPEND(last_proc->instrs, instr);
    } break;

    case TT_ELIF: {
      IrArg arg0 = parser_parse_arg(&parser);
      RelOp rel_op = parser_parse_rel_op(&parser);
      IrArg arg1 = parser_parse_arg(&parser);

      parser_expect_token(&parser, MASK(TT_COLON));

      if (parser.blocks.len > 0) {
        Block *last_block = parser.blocks.items + --parser.blocks.len;

        if (last_block->kind != BlockKindIf) {
          ERROR("`else` not inside of `if`\n");
        }

        Str label_name = last_block->end_label_name;
        IrInstr instr = { IrInstrKindLabel, { .label = { label_name } } };
        DA_APPEND(last_proc->instrs, instr);
      } else {
        ERROR("`elif` without `if`\n");
        exit(1);
      }

      Str end_label_name = gen_label_name(parser.max_labels_count++);
      Block new_block = { BlockKindIf, end_label_name };
      DA_APPEND(parser.blocks, new_block);

      IrInstr instr = { IrInstrKindIf, { ._if = { arg0, arg1, rel_op, end_label_name } } };
      DA_APPEND(last_proc->instrs, instr);
    } break;

    case TT_ELSE: {
      parser_expect_token(&parser, MASK(TT_COLON));

      if (parser.blocks.len > 0) {
        Block *last_block = parser.blocks.items + --parser.blocks.len;

        if (last_block->kind != BlockKindIf) {
          ERROR("`else` not inside of `if`\n");
        }

        Str label_name = last_block->end_label_name;
        IrInstr instr = { IrInstrKindLabel, { .label = { label_name } } };
        DA_APPEND(last_proc->instrs, instr);
      } else {
        ERROR("`else` without `if`\n");
        exit(1);
      }

      Str end_label_name = gen_label_name(parser.max_labels_count++);
      Block new_block = { BlockKindIf, end_label_name };
      DA_APPEND(parser.blocks, new_block);
    } break;

    case TT_END: {
      if (parser.blocks.len > 0) {
        if (parser.blocks.items[--parser.blocks.len].kind != BlockKindProc) {
          Str label_name = parser.blocks.items[parser.blocks.len].end_label_name;
          IrInstr instr = { IrInstrKindLabel, { .label = { label_name } } };
          DA_APPEND(last_proc->instrs, instr);
        }
      }

      if (recursion_level == 0)
        last_proc = NULL;
      else
        --recursion_level;
    } break;

    case TT_RETVAL: {
      IrArg arg = parser_parse_arg(&parser);

      IrInstr instr = { IrInstrKindRetVal, { .ret_val = { arg } } };
      DA_APPEND(last_proc->instrs, instr);
    } break;

    default: {
      ERROR("Wrong token id at %u:%u\n",
            token->row + 1, token->col + 1);
      exit(1);
    }
    }

    token = parser_next_token(&parser);
  }

  if (parser.blocks.len > 0) {
    ERROR("%u blocks were not closed\n", parser.blocks.len);
    exit(1);
  }

  return ir;
}
