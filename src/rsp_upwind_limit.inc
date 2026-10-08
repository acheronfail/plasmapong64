/* Bound combined flow before the advection consumer reads it. Most blocks
 * need no reciprocal or writes. Pure-axis flow clips exactly; diagonal flow
 * uses a guarded hardware reciprocal and retains subpixel Q4 storage. */
Limit:
    move s1,a0
    move s2,a1
    mtc2 a2,$v20.e0
    sll t3,a3,1
    mtc2 t3,$v20.e1
    addiu t3,a2,1
    mtc2 t3,$v20.e2
    li s5,HEIGHT
    ctc2 zero,COP2_CTRL_VCO
limit_row:
    move s0,s1
    li s4,%lo(urow)
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
    move s0,s2
    li s4,%lo(vrow)
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
    move s6,zero
    move a3,zero
limit_batch:
    addiu t4,s6,%lo(urow)
    addiu t5,s6,%lo(vrow)
    lqv $v01,0,t4
    lqv $v02,0,t5
    vabs $v03,$v01,$v01
    vabs $v04,$v02,$v02
    vadd $v05,$v03,$v04
    vge $v06,$v05,$v20.e2
    cfc2 t3,COP2_CTRL_VCC
    andi t3,t3,255
    beqz t3,limit_next_batch
    nop
    li a3,1
    .set lane,0
    /* Scalar divider instructions need named element literals. */
    vrcp $v07.e0,$v05.e0
    vrcph $v08.e0,vzero.e0
    vrcp $v07.e1,$v05.e1
    vrcph $v08.e1,vzero.e0
    vrcp $v07.e2,$v05.e2
    vrcph $v08.e2,vzero.e0
    vrcp $v07.e3,$v05.e3
    vrcph $v08.e3,vzero.e0
    vrcp $v07.e4,$v05.e4
    vrcph $v08.e4,vzero.e0
    vrcp $v07.e5,$v05.e5
    vrcph $v08.e5,vzero.e0
    vrcp $v07.e6,$v05.e6
    vrcph $v08.e6,vzero.e0
    vrcp $v07.e7,$v05.e7
    vrcph $v08.e7,vzero.e0
    vmudn $v09,$v07,$v03
    vmadh $v09,$v08,$v03
    vmudm $v09,$v09,$v20.e1
    vmudn $v10,$v07,$v04
    vmadh $v10,$v08,$v04
    vmudm $v10,$v10,$v20.e1
    vlt $v11,$v03,$v20.e0
    veq $v12,$v04,vzero
    vmrg $v09,$v11,$v09
    vlt $v11,$v04,$v20.e0
    veq $v12,$v03,vzero
    vmrg $v10,$v11,$v10
    vabs $v09,$v01,$v09
    vabs $v10,$v02,$v10
    vge $v06,$v05,$v20.e2
    vmrg $v09,$v09,$v01
    vmrg $v10,$v10,$v02
    sqv $v09,0,t4
    sqv $v10,0,t5
limit_next_batch:
    addiu s6,s6,16
    slti t3,s6,ROW_BYTES
    bnez t3,limit_batch
    nop
    beqz a3,limit_next_row
    nop
    move s0,s1
    li s4,%lo(urow)
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT
    move s0,s2
    li s4,%lo(vrow)
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT
limit_next_row:
    addiu s1,s1,ROW_BYTES
    addiu s2,s2,ROW_BYTES
    addiu s5,s5,-1
    bnez s5,limit_row
    nop
    j RSPQ_Loop
    nop
