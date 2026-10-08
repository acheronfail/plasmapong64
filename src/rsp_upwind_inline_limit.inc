/* Limit one cached velocity row before any advection neighbour reads it.
 * t8/t9 = DMEM u/v rows; v1 = RDRAM u row (v is one plane later).
 * Preserve source-ring offset, row offset and return address. Constants v21
 * are disjoint from advection's step/rounding registers. */
ClampLoadedRow:
    sw s6,%lo(limit_saved)(zero)
    sw a3,%lo(limit_saved)+4(zero)
    sw ra,%lo(limit_saved)+8(zero)
    move s6,zero
    move a3,zero
inline_limit_batch:
    addu t4,s6,t8
    addu t5,s6,t9
    lqv $v01,0,t4
    lqv $v02,0,t5
    vabs $v03,$v01,$v01
    vabs $v04,$v02,$v02
    vadd $v05,$v03,$v04
    vge $v06,$v05,$v21.e2
    cfc2 t3,COP2_CTRL_VCC
    andi t3,t3,255
    beqz t3,inline_limit_next_batch
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
    vmudm $v09,$v09,$v21.e1
    vmudn $v10,$v07,$v04
    vmadh $v10,$v08,$v04
    vmudm $v10,$v10,$v21.e1
    vlt $v11,$v03,$v21.e0
    veq $v12,$v04,vzero
    vmrg $v09,$v11,$v09
    vlt $v11,$v04,$v21.e0
    veq $v12,$v03,vzero
    vmrg $v10,$v11,$v10
    vabs $v09,$v01,$v09
    vabs $v10,$v02,$v10
    vge $v06,$v05,$v21.e2
    vmrg $v09,$v09,$v01
    vmrg $v10,$v10,$v02
    sqv $v09,0,t4
    sqv $v10,0,t5
inline_limit_next_batch:
    addiu s6,s6,16
    slti t3,s6,ROW_BYTES
    bnez t3,inline_limit_batch
    nop
    beqz a3,inline_limit_done
    nop
    move s0,v1
    move s4,t8
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT_ASYNC
    addiu s0,v1,PLANE_BYTES
    move s4,t9
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT_ASYNC
inline_limit_done:
    lw s6,%lo(limit_saved)(zero)
    lw a3,%lo(limit_saved)+4(zero)
    lw ra,%lo(limit_saved)+8(zero)
    jr ra
    nop
