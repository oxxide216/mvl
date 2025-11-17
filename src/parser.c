#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "mvm/src/misc.h"
#include "lexgen/runtime-src/runtime.h"
#include "../grammar.h"
#include "ir.h"
#include "ir_to_mvm.h"
#include "shl/shl-log.h"
#include "shl/shl-arena.h"

typedef enum {
  BlockKindProc = 0,
  BlockKindIf,
  BlockKindWhile,
} BlockKind;

typedef struct {
  BlockKind kind;
  Str       begin_label_name;
  Str       end_label_name;
} Block;

typedef Da(Block) Blocks;

typedef struct {
  Str    name;
  Type  *type;
} Var;

typedef Da(Var) Vars;

typedef struct {
  Str        name;
  IrArgValue value;
} ParserStaticVariable;

typedef Da(ParserStaticVariable) ParserStaticVariables;

typedef struct {
  Str  name;
  u8  *data;
  u32  size;
} ParserStaticBuffer;

typedef Da(ParserStaticBuffer) ParserStaticData;

typedef struct {
  Tokens                *tokens;
  u32                    index;
  Blocks                 blocks;
  ParserStaticVariables  static_vars;
  ParserStaticData       static_data;
  u32                    max_labels_count;
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
  STR_LIT("`record`"),
  STR_LIT("`inline`"),
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
  STR_LIT("'*'"),
  STR_LIT("`$`"),
  STR_LIT("operator"),
};

static Type unit_type = { TypeKindUnit, NULL };

static void parser_parse_proc_instrs(Parser *parser, IrInstrs *instrs);
static bool parser_parse_global_instr(Parser *parser, Ir *ir,
                                      Token *begin_token,
                                      bool is_in_proc);

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
    ERROR(STR_FMT": Expected ",
          STR_ARG(token->file_path));
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
  fputs(", but got `", stderr);
  str_fprint(stderr, token->lexeme);
  fputs("`\n", stderr);

  exit(1);
}

static Token *parser_peek_token(Parser *parser, u32 offset) {
  if (parser->index + offset >= parser->tokens->len)
    return NULL;

  return parser->tokens->items + parser->index + offset;
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

static IrArgValue token_to_number_ir_arg_value(Token *token) {
  Str str = token->lexeme;

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

    if (token->id == TT_CHAR_LIT) {
      number = str.ptr[0];
      ++str.ptr;
      --str.len;
    }

    if (str.len == 0)
      str = STR_LIT("s64");

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
      PERROR(STR_FMT":%u:%u: ", "Unknown type name: "STR_FMT"\n",
             STR_ARG(token->file_path),
             token->row + 1, token->col + 1,
             STR_ARG(str));
      exit(1);
    }
    }
  }

  type->kind = TypeKindS64;
  return (IrArgValue) { type, { ._s64 = number } };
}

static IrArg token_to_ir_arg(Token *token, IrArgKind kind) {
  IrArg ir_arg;
  ir_arg.kind = kind;

  switch (kind) {
  case IrArgKindValue: {
    ir_arg.as.value = token_to_number_ir_arg_value(token);
  } break;

  case IrArgKindVar: {
    ir_arg.as.var = token->lexeme;
  } break;

  default: {
    PERROR(STR_FMT"%u:%u: ", "Wrong argument kind\n",
           STR_ARG(token->file_path),
           token->row + 1, token->col + 1);
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

static Str create_static_var_name(u32 id) {
  StringBuilder sb = {0};
  sb_push(&sb, "?s");
  sb_push_u32(&sb, id);

  return sb_to_str(sb);
}

static IrArg parser_parse_arg(Parser *parser) {
  Token *token = parser_expect_token(parser, MASK(TT_NUMBER) | MASK(TT_IDENT) |
                                             MASK(TT_STR_LIT) | MASK(TT_CHAR_LIT));
  IrArg arg;

  if (token->id == TT_NUMBER || token->id == TT_CHAR_LIT) {
    arg = token_to_ir_arg(token, IrArgKindValue);
  } else if (token->id == TT_IDENT) {
    arg = token_to_ir_arg(token, IrArgKindVar);
  } else if (token->id == TT_STR_LIT) {
    Str buffer_name = create_static_var_name(parser->static_data.len);
    u8 *data = malloc(token->lexeme.len + 1);
    memcpy(data, token->lexeme.ptr, token->lexeme.len);
    data[token->lexeme.len] = '\0';
    ParserStaticBuffer buffer = {
      buffer_name,
      data,
      token->lexeme.len + 1,
    };
    DA_APPEND(parser->static_data, buffer);

    Token str_token = {
      buffer_name,
      token->id,
      token->row,
      token->col,
      token->file_path,
    };

    arg = token_to_ir_arg(&str_token, IrArgKindVar);
  }

  return arg;
}

static IrProc parser_parse_proc_def(Parser *parser) {
  IrProc proc = {0};

  Token *token = parser_expect_token(parser, MASK(TT_IDENT) |
                                             MASK(TT_NAKED) |
                                             MASK(TT_INLINE));

  if (token->id == TT_NAKED) {
    proc.is_naked = true;
    token = parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_INLINE));
  }

  if (token->id == TT_INLINE) {
    proc.is_inlined = true;
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

    token = parser_peek_token(parser, 0);
    if (token->id != TT_CPAREN)
      parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CPAREN));
  }

  parser_expect_token(parser, MASK(TT_CPAREN));

  token = parser_peek_token(parser, 0);
  if (token->id == TT_RIGHT_ARROW) {
    parser_next_token(parser);
    proc.ret_val_type = parser_parse_type(parser);
  } else {
    proc.ret_val_type = &unit_type;
  }

  parser_expect_token(parser, MASK(TT_COLON));

  return proc;
}

static IrInstr parser_parse_proc_call(Parser *parser, Str name, Str dest) {
  IrArgs args = {0};

  Token *token = parser_peek_token(parser, 0);
  while (token && token->id != TT_CPAREN) {
    IrArg arg = parser_parse_arg(parser);
    DA_APPEND(args, arg);

    token = parser_peek_token(parser, 0);
    if (token->id != TT_CPAREN)
      parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CPAREN));
  }

  parser_expect_token(parser, MASK(TT_CPAREN));

  return (IrInstr) { IrInstrKindCall, { .call = { name, dest, args } } };
}

static RelOp parser_parse_rel_op(Parser *parser) {
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

static Str gen_label_name(u32 index) {
  StringBuilder sb = {0};
  sb_push(&sb, "label");
  sb_push_u32(&sb, index);
  return sb_to_str(sb);
}

static IrInstr parser_parse_asm(Parser *parser, Str dest, Type *dest_type) {
  Token *code_token = parser_expect_token(parser, MASK(TT_STR_LIT));
  VarNames var_names = {0};

  parser_expect_token(parser, MASK(TT_OBRACKET));

  Token *token = parser_peek_token(parser, 0);
  while (token && token->id != TT_CBRACKET) {
    Token *var_name_token = parser_expect_token(parser, MASK(TT_IDENT));
    DA_APPEND(var_names, var_name_token->lexeme);

    token = parser_peek_token(parser, 0);
    if (token->id != TT_CBRACKET)
      token = parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CBRACKET));
  }

  parser_expect_token(parser, MASK(TT_CBRACKET));

  return (IrInstr) {
    IrInstrKindAsm,
    { ._asm = { dest, dest_type, code_token->lexeme, var_names } },
  };
}

static void parser_parse_proc_instrs(Parser *parser, IrInstrs *instrs) {
  u32 recursion_level = 0;

  Token *token = parser_next_token(parser);
  while (token) {
    bool instr_is_global = parser_parse_global_instr(parser, NULL, token, true);
    if (instr_is_global) {
      token = parser_next_token(parser);
      continue;
    }

    switch (token->id) {
    case TT_IDENT: {
      Token *next = parser_expect_token(parser, MASK(TT_COLON) | MASK(TT_ASSIGN) |
                                                MASK(TT_OPAREN));

      if (next->id == TT_COLON) {
        Type *dest_type = parser_parse_type(parser);
        IrInstr instr = { IrInstrKindCreate, { .create = { token->lexeme, dest_type } } };
        DA_APPEND(*instrs, instr);
      } else if (next->id == TT_ASSIGN) {
        next = parser_peek_token(parser, 0);
        if (next->id == TT_IDENT) {
          next = parser_peek_token(parser, 1);
          if (next->id == TT_OPAREN) {
            Token *callee_name_token = parser_next_token(parser);
            parser_next_token(parser);
            IrInstr instr = parser_parse_proc_call(parser, callee_name_token->lexeme, token->lexeme);
            DA_APPEND(*instrs, instr);
          } else {
            IrArg arg0 = parser_parse_arg(parser);

            Token *op_token = NULL;
            if (next->id == TT_REF || next->id == TT_DEREF || next->id == TT_OP)
              op_token = parser_next_token(parser);

            if (op_token) {
              IrArg arg1 = parser_parse_arg(parser);
              IrInstr instr = {
                IrInstrKindBinOp,
                { .bin_op = { token->lexeme, op_token->lexeme, arg0, arg1 } },
              };
              DA_APPEND(*instrs, instr);
            } else {
              IrInstr instr = { IrInstrKindAssign, { .assign = { token->lexeme, arg0 } } };
              DA_APPEND(*instrs, instr);
            }
          }
        } else if (next->id == TT_ASM) {
          parser_next_token(parser);
          Type *dest_type = parser_parse_type(parser);

          IrInstr instr = parser_parse_asm(parser, token->lexeme, dest_type);
          DA_APPEND(*instrs, instr);
        } else if (next->id == TT_REF || next->id == TT_OP) {
          Token *op_token = parser_next_token(parser);
          IrArg arg = parser_parse_arg(parser);
          IrInstr instr = {
            IrInstrKindUnOp,
            { .un_op = { token->lexeme, op_token->lexeme, arg } },
          };
          DA_APPEND(*instrs, instr);
        } else if (next->id == TT_CAST) {
          parser_next_token(parser);
          Type *type = parser_parse_type(parser);
          IrArg arg = parser_parse_arg(parser);
          IrInstr instr = {
            IrInstrKindCast,
            { .cast = { token->lexeme, type, arg } },
          };
          DA_APPEND(*instrs, instr);
        } else if (next->id == TT_DEREF) {
          parser_next_token(parser);
          Type *type = parser_parse_type(parser);
          IrArg arg = parser_parse_arg(parser);
          IrInstr instr = {
            IrInstrKindDeref,
            { .deref = { token->lexeme, type, arg } },
          };
          DA_APPEND(*instrs, instr);
        } else {
          IrArg arg0 = parser_parse_arg(parser);

          Token *op_token = NULL;
          next = parser_peek_token(parser, 0);
          if (next->id == TT_REF || next->id == TT_DEREF || next->id == TT_OP)
            op_token = parser_next_token(parser);

          if (op_token) {
            IrArg arg1 = parser_parse_arg(parser);
            IrInstr instr = {
              IrInstrKindBinOp,
              { .bin_op = { token->lexeme, op_token->lexeme, arg0, arg1 } },
            };
            DA_APPEND(*instrs, instr);
          } else {
            IrInstr instr = { IrInstrKindAssign, { .assign = { token->lexeme, arg0 } } };
            DA_APPEND(*instrs, instr);
          }
        }
      } else if (next->id == TT_OPAREN) {
        IrInstr instr = parser_parse_proc_call(parser, token->lexeme, (Str) {0});
        DA_APPEND(*instrs, instr);
      }
    } break;

    case TT_IF:
    case TT_WHILE: {
      IrArg arg0 = parser_parse_arg(parser);
      RelOp rel_op = parser_parse_rel_op(parser);
      IrArg arg1 = parser_parse_arg(parser);

      parser_expect_token(parser, MASK(TT_COLON));

      ++recursion_level;

      Str end_label_name = gen_label_name(parser->max_labels_count++);
      Block new_block;
      IrInstr instr;

      if (token->id == TT_IF) {
        new_block = (Block) { BlockKindIf, {0}, end_label_name };
        instr = (IrInstr) {
          IrInstrKindIf,
          {
            ._if = {
              arg0,
              arg1,
              rel_op,
              end_label_name,
            },
          },
        };
      } else {
        Str begin_label_name = gen_label_name(parser->max_labels_count++);
        new_block = (Block) { BlockKindWhile, begin_label_name, end_label_name };
        instr = (IrInstr) {
          IrInstrKindWhile, {
            ._while = {
              arg0,
              arg1,
              rel_op,
              begin_label_name,
              end_label_name,
            },
          },
        };
      }

      DA_APPEND(parser->blocks, new_block);
      DA_APPEND(*instrs, instr);
    } break;

    case TT_ELIF: {
      IrArg arg0 = parser_parse_arg(parser);
      RelOp rel_op = parser_parse_rel_op(parser);
      IrArg arg1 = parser_parse_arg(parser);

      parser_expect_token(parser, MASK(TT_COLON));

      if (parser->blocks.len == 0) {
        ERROR("`elif` without `if`\n");
        exit(1);
      }

      Block *last_block = parser->blocks.items + --parser->blocks.len;

      if (last_block->kind != BlockKindIf) {
        ERROR("`else` not inside of `if`\n");
      }

      Str label_name = last_block->end_label_name;
      IrInstr label_instr = { IrInstrKindLabel, { .label = { label_name } } };
      DA_APPEND(*instrs, label_instr);

      Str end_label_name = gen_label_name(parser->max_labels_count++);
      Block new_block = { BlockKindIf, {0}, end_label_name };
      DA_APPEND(parser->blocks, new_block);

      IrInstr if_instr = { IrInstrKindIf, { ._if = { arg0, arg1, rel_op, end_label_name } } };
      DA_APPEND(*instrs, if_instr);
    } break;

    case TT_ELSE: {
      parser_expect_token(parser, MASK(TT_COLON));

      if (parser->blocks.len == 0) {
        ERROR("`else` without `if`\n");
        exit(1);
      }

      Block *last_block = parser->blocks.items + --parser->blocks.len;
      if (last_block->kind != BlockKindIf) {
        ERROR("`else` not inside of `if`\n");
        exit(1);
      }

      Str label_name = last_block->end_label_name;
      IrInstr instr = { IrInstrKindLabel, { .label = { label_name } } };
      DA_APPEND(*instrs, instr);

      Str end_label_name = gen_label_name(parser->max_labels_count++);
      Block new_block = { BlockKindIf, {0}, end_label_name };
      DA_APPEND(parser->blocks, new_block);
    } break;

    case TT_END: {
      if (parser->blocks.len == 0)
        return;

      Block *last_block = parser->blocks.items + --parser->blocks.len;
      if (last_block->kind != BlockKindProc) {
        if (last_block->kind == BlockKindWhile) {
          Str label_name = last_block->begin_label_name;
          IrInstr instr = { IrInstrKindJump, { .label = { label_name } } };
          DA_APPEND(*instrs, instr);
        }

        Str label_name = last_block->end_label_name;
        IrInstr instr = { IrInstrKindLabel, { .label = { label_name } } };
        DA_APPEND(*instrs, instr);
      }
    } break;

    case TT_BREAK:
    case TT_CONTINUE: {
      Block *loop_block = NULL;
      for (u32 i = parser->blocks.len; i > 0; --i) {
        if (parser->blocks.items[i - 1].kind == BlockKindWhile) {
          loop_block = parser->blocks.items + i - 1;
          break;
        }
      }

      if (!loop_block) {
        if (token->id == TT_BREAK)
          ERROR("`break` not inside of a loop\n");
        else
          ERROR("`continue` not inside of a loop\n");
        exit(1);
      }

      IrInstr instr;
      if (token->id == TT_BREAK) {
        Str label_name = loop_block->end_label_name;
        instr = (IrInstr) { IrInstrKindJump, { .jump = { label_name } } };
      } else {
        Str label_name = loop_block->begin_label_name;
        instr = (IrInstr) { IrInstrKindJump, { .jump = { label_name } } };
      }
      DA_APPEND(*instrs, instr);
    } break;

    case TT_RET: {
      IrInstr instr = { IrInstrKindRet, {} };
      DA_APPEND(*instrs, instr);
    } break;

    case TT_RETVAL: {
      IrArg arg = parser_parse_arg(parser);
      IrInstr instr = { IrInstrKindRetVal, { .ret_val = { arg } } };
      DA_APPEND(*instrs, instr);
    } break;

    case TT_INCLUDE: {} break;

    case TT_STATIC: {} break;

    case TT_ASM: {
      IrInstr instr = parser_parse_asm(parser, (Str) {0}, &unit_type);
      DA_APPEND(*instrs, instr);
    } break;

    case TT_RECORD: {} break;

    case TT_REF:
    case TT_DEREF:
    case TT_OP: {
      Token *dest_token = parser_expect_token(parser, MASK(TT_IDENT));
      parser_expect_token(parser, MASK(TT_ASSIGN));
      IrArg arg = parser_parse_arg(parser);

      IrInstr instr = {
        IrInstrKindPreAssignOp,
        { .pre_assign_op = { dest_token->lexeme, token->lexeme, arg } },
      };
      DA_APPEND(*instrs, instr);
    } break;

    default: {
      ERROR(STR_FMT":%u:%u: Unexpected statement begin token: `"STR_FMT"`\n",
            STR_ARG(token->file_path), token->row + 1,
            token->col + 1, STR_ARG(token->lexeme));
      exit(1);
    }
    }

    token = parser_next_token(parser);
  }

  if (parser->blocks.len > 0) {
    ERROR("%u blocks were not closed\n", parser->blocks.len);
    exit(1);
  }
}

static bool parser_parse_global_instr(Parser *parser, Ir *ir,
                                      Token *begin_token,
                                      bool is_in_proc) {
  switch (begin_token->id) {
    case TT_PROC: {
      if (is_in_proc) {
        ERROR(STR_FMT":%u:%u: Nested procedures are not supported\n",
          STR_ARG(begin_token->file_path), begin_token->row + 1, begin_token->col + 1);
        exit(1);
      }

      IrProc new_proc = parser_parse_proc_def(parser);
      parser_parse_proc_instrs(parser, &new_proc.instrs);
      DA_APPEND(ir->procs, new_proc);
    } break;

    case TT_STATIC: {
      Token *name_token = parser_expect_token(parser, MASK(TT_IDENT));
      parser_expect_token(parser, MASK(TT_ASSIGN));
      IrArg arg = parser_parse_arg(parser);
      if (arg.kind == IrArgKindVar) {
        ERROR("Only value can be assigned to a static variable\n");
        exit(1);
      }

      ParserStaticVariable static_var = { name_token->lexeme, arg.as.value };
      DA_APPEND(parser->static_vars, static_var);
    } break;

    case TT_RECORD: {} break;

    case TT_INCLUDE: {
      parser_expect_token(parser, MASK(TT_STR_LIT));
    } break;

    default: return false;
  }

  return true;
}

static Ir parser_parse(Parser *parser) {
  Ir ir = {0};

  Token *token = parser_next_token(parser);
  while (token) {
    bool instr_is_global = parser_parse_global_instr(parser, &ir, token, false);
    if (!instr_is_global) {
      ERROR(STR_FMT":%u:%u: Instrucion cannot be defined outside of a procedure\n",
        STR_ARG(token->file_path), token->row + 1, token->col + 1);
      exit(1);
    }

    token = parser_next_token(parser);
  }

  return ir;
}

Ir parse(Tokens *tokens) {
  Parser parser = {0};
  parser.tokens = tokens;

  Ir ir = parser_parse(&parser);

  for (u32 i = 0; i < parser.static_vars.len; ++i) {
    Str name = parser.static_vars.items[i].name;
    IrArgValue value = parser.static_vars.items[i].value;
    StaticVariable var = { name, ir_arg_value_to_value(&value) };
    DA_APPEND(ir.static_vars, var);
  }

  for (u32 i = 0; i < parser.static_data.len; ++i) {
    StaticBuffer buffer = {
      parser.static_data.items[i].name,
      parser.static_data.items[i].data,
      parser.static_data.items[i].size,
    };
    DA_APPEND(ir.static_data, buffer);
  }

  return ir;
}
