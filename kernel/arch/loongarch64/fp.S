    .macro fp_save_csr_cc
    movfcsr2gr $t0, $fcsr0
    st.d $t0, $a0, 0

    movcf2gr  $t0, $fcc0
    movcf2gr  $t1, $fcc1
    bstrins.d $t0, $t1, 15, 8
    movcf2gr  $t1, $fcc2
    bstrins.d $t0, $t1, 23, 16
    movcf2gr  $t1, $fcc3
    bstrins.d $t0, $t1, 31, 24
    movcf2gr  $t1, $fcc4
    bstrins.d $t0, $t1, 39, 32
    movcf2gr  $t1, $fcc5
    bstrins.d $t0, $t1, 47, 40
    movcf2gr  $t1, $fcc6
    bstrins.d $t0, $t1, 55, 48
    movcf2gr  $t1, $fcc7
    bstrins.d $t0, $t1, 63, 56
    st.d $t0, $a0, 8
    .endm

    .macro fp_restore_csr_cc
    ld.d $t0, $a0, 8
    bstrpick.d $t1, $t0, 7, 0
    movgr2cf $fcc0, $t1    
    bstrpick.d $t1, $t0, 15, 8
    movgr2cf $fcc1, $t1
    bstrpick.d $t1, $t0, 23, 16
    movgr2cf $fcc2, $t1
    bstrpick.d $t1, $t0, 31, 24
    movgr2cf $fcc3, $t1
    bstrpick.d $t1, $t0, 39, 32
    movgr2cf $fcc4, $t1
    bstrpick.d $t1, $t0, 47, 40
    movgr2cf $fcc5, $t1    
    bstrpick.d $t1, $t0, 55, 48
    movgr2cf $fcc6, $t1
    bstrpick.d $t1, $t0, 63, 56
    movgr2cf $fcc7, $t1

    ld.d $t0, $a0, 0
    movgr2fcsr $fcsr0, $t0
    .endm
   

    .text
    .type loongarch_restore_fp, @function
    .globl loongarch_restore_fp
loongarch_restore_fp:
    fld.d $f0, $a0, 16
    fld.d $f1, $a0, 24
    fld.d $f2, $a0, 32
    fld.d $f3, $a0, 40
    fld.d $f4, $a0, 48
    fld.d $f5, $a0, 56
    fld.d $f6, $a0, 64
    fld.d $f7, $a0, 72
    fld.d $f8, $a0, 80
    fld.d $f9, $a0, 88
    fld.d $f10, $a0, 96
    fld.d $f11, $a0, 104
    fld.d $f12, $a0, 112
    fld.d $f13, $a0, 120
    fld.d $f14, $a0, 128
    fld.d $f15, $a0, 136
    fld.d $f16, $a0, 144
    fld.d $f17, $a0, 152
    fld.d $f18, $a0, 160
    fld.d $f19, $a0, 168
    fld.d $f20, $a0, 176
    fld.d $f21, $a0, 184
    fld.d $f22, $a0, 192
    fld.d $f23, $a0, 200
    fld.d $f24, $a0, 208
    fld.d $f25, $a0, 216
    fld.d $f26, $a0, 224
    fld.d $f27, $a0, 232
    fld.d $f28, $a0, 240
    fld.d $f29, $a0, 248
    fld.d $f30, $a0, 256
    fld.d $f31, $a0, 264
    fp_restore_csr_cc
    ret

    .type loongarch_save_fp, @function
    .globl loongarch_save_fp
loongarch_save_fp:
    fp_save_csr_cc
    fst.d $f0, $a0, 16
    fst.d $f1, $a0, 24
    fst.d $f2, $a0, 32
    fst.d $f3, $a0, 40
    fst.d $f4, $a0, 48
    fst.d $f5, $a0, 56
    fst.d $f6, $a0, 64
    fst.d $f7, $a0, 72
    fst.d $f8, $a0, 80
    fst.d $f9, $a0, 88
    fst.d $f10, $a0, 96
    fst.d $f11, $a0, 104
    fst.d $f12, $a0, 112
    fst.d $f13, $a0, 120
    fst.d $f14, $a0, 128
    fst.d $f15, $a0, 136
    fst.d $f16, $a0, 144
    fst.d $f17, $a0, 152
    fst.d $f18, $a0, 160
    fst.d $f19, $a0, 168
    fst.d $f20, $a0, 176
    fst.d $f21, $a0, 184
    fst.d $f22, $a0, 192
    fst.d $f23, $a0, 200
    fst.d $f24, $a0, 208
    fst.d $f25, $a0, 216
    fst.d $f26, $a0, 224
    fst.d $f27, $a0, 232
    fst.d $f28, $a0, 240
    fst.d $f29, $a0, 248
    fst.d $f30, $a0, 256
    fst.d $f31, $a0, 264
    ret


    .type loongarch_restore_lsx, @function
    .globl loongarch_restore_lsx
loongarch_restore_lsx:
    vld $vr0, $a0, 16
    vld $vr1, $a0, 32
    vld $vr2, $a0, 48
    vld $vr3, $a0, 64
    vld $vr4, $a0, 80
    vld $vr5, $a0, 96
    vld $vr6, $a0, 112
    vld $vr7, $a0, 128
    vld $vr8, $a0, 144
    vld $vr9, $a0, 160
    vld $vr10, $a0, 176
    vld $vr11, $a0, 192
    vld $vr12, $a0, 208
    vld $vr13, $a0, 224
    vld $vr14, $a0, 240
    vld $vr15, $a0, 256
    vld $vr16, $a0, 272
    vld $vr17, $a0, 288
    vld $vr18, $a0, 304
    vld $vr19, $a0, 320
    vld $vr20, $a0, 336
    vld $vr21, $a0, 352
    vld $vr22, $a0, 368
    vld $vr23, $a0, 384
    vld $vr24, $a0, 400
    vld $vr25, $a0, 416
    vld $vr26, $a0, 432
    vld $vr27, $a0, 448
    vld $vr28, $a0, 464
    vld $vr29, $a0, 480
    vld $vr30, $a0, 496
    vld $vr31, $a0, 512
    fp_restore_csr_cc
    ret


    .type loongarch_save_lsx, @function
    .globl loongarch_save_lsx
loongarch_save_lsx:
    fp_save_csr_cc
    vst $vr0, $a0, 16
    vst $vr1, $a0, 32
    vst $vr2, $a0, 48
    vst $vr3, $a0, 64
    vst $vr4, $a0, 80
    vst $vr5, $a0, 96
    vst $vr6, $a0, 112
    vst $vr7, $a0, 128
    vst $vr8, $a0, 144
    vst $vr9, $a0, 160
    vst $vr10, $a0, 176
    vst $vr11, $a0, 192
    vst $vr12, $a0, 208
    vst $vr13, $a0, 224
    vst $vr14, $a0, 240
    vst $vr15, $a0, 256
    vst $vr16, $a0, 272
    vst $vr17, $a0, 288
    vst $vr18, $a0, 304
    vst $vr19, $a0, 320
    vst $vr20, $a0, 336
    vst $vr21, $a0, 352
    vst $vr22, $a0, 368
    vst $vr23, $a0, 384
    vst $vr24, $a0, 400
    vst $vr25, $a0, 416
    vst $vr26, $a0, 432
    vst $vr27, $a0, 448
    vst $vr28, $a0, 464
    vst $vr29, $a0, 480
    vst $vr30, $a0, 496
    vst $vr31, $a0, 512
    ret


    .type loongarch_restore_lasx, @function
    .globl loongarch_restore_lasx
loongarch_restore_lasx:
    xvld $xr0, $a0, 16
    xvld $xr1, $a0, 48
    xvld $xr2, $a0, 80
    xvld $xr3, $a0, 112
    xvld $xr4, $a0, 144
    xvld $xr5, $a0, 176
    xvld $xr6, $a0, 208
    xvld $xr7, $a0, 240
    xvld $xr8, $a0, 272
    xvld $xr9, $a0, 304
    xvld $xr10, $a0, 336
    xvld $xr11, $a0, 368
    xvld $xr12, $a0, 400
    xvld $xr13, $a0, 432
    xvld $xr14, $a0, 464
    xvld $xr15, $a0, 496
    xvld $xr16, $a0, 528
    xvld $xr17, $a0, 560
    xvld $xr18, $a0, 592
    xvld $xr19, $a0, 624
    xvld $xr20, $a0, 656
    xvld $xr21, $a0, 688
    xvld $xr22, $a0, 720
    xvld $xr23, $a0, 752
    xvld $xr24, $a0, 784
    xvld $xr25, $a0, 816
    xvld $xr26, $a0, 848
    xvld $xr27, $a0, 880
    xvld $xr28, $a0, 912
    xvld $xr29, $a0, 944
    xvld $xr30, $a0, 976
    xvld $xr31, $a0, 1008
    fp_restore_csr_cc
    ret

    .type loongarch_save_lasx, @function
    .globl loongarch_save_lasx
loongarch_save_lasx:
    fp_save_csr_cc
    xvst $xr0, $a0, 16
    xvst $xr1, $a0, 48
    xvst $xr2, $a0, 80
    xvst $xr3, $a0, 112
    xvst $xr4, $a0, 144
    xvst $xr5, $a0, 176
    xvst $xr6, $a0, 208
    xvst $xr7, $a0, 240
    xvst $xr8, $a0, 272
    xvst $xr9, $a0, 304
    xvst $xr10, $a0, 336
    xvst $xr11, $a0, 368
    xvst $xr12, $a0, 400
    xvst $xr13, $a0, 432
    xvst $xr14, $a0, 464
    xvst $xr15, $a0, 496
    xvst $xr16, $a0, 528
    xvst $xr17, $a0, 560
    xvst $xr18, $a0, 592
    xvst $xr19, $a0, 624
    xvst $xr20, $a0, 656
    xvst $xr21, $a0, 688
    xvst $xr22, $a0, 720
    xvst $xr23, $a0, 752
    xvst $xr24, $a0, 784
    xvst $xr25, $a0, 816
    xvst $xr26, $a0, 848
    xvst $xr27, $a0, 880
    xvst $xr28, $a0, 912
    xvst $xr29, $a0, 944
    xvst $xr30, $a0, 976
    xvst $xr31, $a0, 1008
    ret