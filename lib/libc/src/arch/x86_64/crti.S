  .section .init
  .globl _init
  _init:
  push %ebp
  movl %esp, %ebp
  # TODO: Save preserved registers :)

  .section .fini
  .globl _fini
 _fini:
  push %ebp
  movl %esp, %ebp