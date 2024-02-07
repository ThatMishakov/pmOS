   .section .init
   .globl _init
   _init:
   push %rbp
   movq %rsp, %rbp
   # TODO: Save preserved registers :)

   .section .fini
   .globl _fini
 _fini:
   push %rbp
   movq %rsp, %rbp