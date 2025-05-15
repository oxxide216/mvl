global _start
section .text
_start:
  mov rdi,qword[rsp]
  lea rsi,qword[rsp+8]
  call main
  mov rdi,rax
  mov rdi,0
  mov rax,60
  syscall
$main:
  push rbx
  mov rdi,2
  mov rsi,2
  call $add
  mov rbx,rax
  mov rdi,rbx
  call $exit
  pop rbx
  ret
$add:
  add rdi,rsi
  mov rax,rdi
  ret
$exit:
  mov rax,60
  syscall
  ret
