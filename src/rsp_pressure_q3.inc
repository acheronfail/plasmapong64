/* Experimental parallel Q3 pressure recurrence; public input/output stay Q12. */
#define WIDTH PLASMAPONG_GRID_W
#define HEIGHT PLASMAPONG_GRID_H
#define SHORT_ROW (WIDTH*2)
#define WORD_ROW (WIDTH*4)
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand PressureSolve,12
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
rows: .ds.b SHORT_ROW*(PLASMAPONG_PRESSURE_PASSES+2)
rows_end:
divrows: .ds.b SHORT_ROW*PLASMAPONG_PRESSURE_PASSES
divrows_end:
div_input: .ds.b WORD_ROW
word_output: .ds.b WORD_ROW
base_buffer: .ds.b 32
short_cursor: .ds.b 4
warm_cursor: .ds.b 4
warm_stride: .ds.b 4
    .text
PressureSolve:
    /* The chained path already retains exact Q3 pressure. Read that field
       directly; the standalone Q12 interface still accepts arbitrary words. */
    addiu t3,a0,WORD_ROW
    li t4,WORD_ROW
    beqz a2,q3_warm_cursor_ready
    nop
    addiu t3,a2,SHORT_ROW
    li t4,SHORT_ROW
q3_warm_cursor_ready:
    sw t3,%lo(warm_cursor)(zero)
    sw t4,%lo(warm_stride)(zero)
    li t3,128
    mtc2 t3,$v22.e0
    vxor $v22,vzero,$v22.e0
    beqz a2,q3_short_cursor_ready
    nop
    addiu a2,a2,SHORT_ROW
q3_short_cursor_ready:
    sw a2,%lo(short_cursor)(zero)
    li t3,32
    mtc2 t3,$v20.e0
    vxor $v20,vzero,$v20.e0
    li t3,8192
    mtc2 t3,$v21.e0
    li t3,2048
    mtc2 t3,$v21.e1
    li t3,512
    mtc2 t3,$v21.e2
    li t3,128
    mtc2 t3,$v21.e3
    ctc2 zero,COP2_CTRL_VCO
    addiu s1,a0,WORD_ROW
    addiu s2,a1,WORD_ROW
    li s3,1
    li fp,%lo(rows)+SHORT_ROW
#if PLASMAPONG_PRESSURE_PASSES == 1
    li k0,%lo(divrows)
#else
    li k0,%lo(divrows)+SHORT_ROW
#endif
    li t3,%lo(rows)
    li t4,%lo(rows_end)
q3_zero:
    sw zero,0(t3)
    addiu t3,t3,4
    bne t3,t4,q3_zero
    nop

q3_tick:
    slti t3,s3,HEIGHT-1
    beqz t3,q3_loaded
    nop
    move s0,s2
    li s4,%lo(div_input)
    li t0,WORD_ROW-1
    li t1,WORD_ROW
    jal DMAExec
    li t2,DMA_IN
    addiu s2,s2,WORD_ROW
    li t3,%lo(div_input)
    move t4,k0
    .set part,0
    .rept WIDTH/4
    lqv $v10,part,t3
    /* (signed high word*32 + unsigned low word/2048), saturated Q3 d/4. */
    vmudh $v11,$v20,$v10.q0
    vmadm $v11,$v20,$v10.q1
    ssv $v11,0,0,t4
    ssv $v11,4,2,t4
    ssv $v11,8,4,t4
    ssv $v11,12,6,t4
    addiu t4,t4,8
    .set part,part+16
    .endr
    /* Seed the first current row once; later current rows were loaded as the
       prior tick's below row. One extra ring slot protects the last sweep's
       above neighbour while this future row is populated. */
    li t3,1
    bne s3,t3,q3_warm_below
    nop
    lw t8,%lo(warm_cursor)(zero)
    move t9,fp
    jal q3_load_warm
    nop
q3_warm_below:
    lw t8,%lo(warm_cursor)(zero)
    lw t3,%lo(warm_stride)(zero)
    addu t8,t8,t3
    addiu t9,fp,SHORT_ROW
    li t3,%lo(rows_end)
    bne t9,t3,q3_warm_target
    nop
    li t9,%lo(rows)
q3_warm_target:
    jal q3_load_warm
    nop
    lw t8,%lo(warm_cursor)(zero)
    lw t3,%lo(warm_stride)(zero)
    addu t8,t8,t3
    sw t8,%lo(warm_cursor)(zero)
q3_loaded:
    move s5,zero
    move s6,fp
    move s7,k0
q3_pass:
    subu v0,s3,s5
    blez v0,q3_next_tick
    nop
    slti t3,v0,HEIGHT-1
    beqz t3,q3_next_pass
    nop
    addiu k1,s6,-SHORT_ROW
    li t3,%lo(rows)
    sltu t4,k1,t3
    beqz t4,q3_above
    nop
    li k1,%lo(rows_end)-SHORT_ROW
q3_above:
    addiu a2,s6,SHORT_ROW
    li t3,%lo(rows_end)
    bne a2,t3,q3_below
    nop
    li a2,%lo(rows)
q3_below:
    li t3,1
    bne v0,t3,q3_not_top
    nop
    move k1,s6
q3_not_top:
    li t3,HEIGHT-2
    bne v0,t3,q3_not_bottom
    nop
    move a2,s6
q3_not_bottom:
q3_old_row_ready:
    lh t3,0(s6)
    vxor $v08,vzero,vzero
    mtc2 t3,$v08.e7
    li t3,%lo(base_buffer)
    sqv $v08,0,t3
    addiu a0,s6,2
    addiu a1,s7,2
    addiu k1,k1,2
    addiu a2,a2,2
    li v1,WIDTH/8
.macro q3_window vector, base, offset
    addiu t3,\base,\offset
    lqv \vector,0,t3
    lrv \vector,16,t3
.endm
q3_batch:
    q3_window $v02,k1,0
    q3_window $v03,a2,0
    q3_window $v04,a0,2
    q3_window $v05,a1,0
    q3_window $v06,a0,-8
    li t3,WIDTH/8
    bne v1,t3,q3_tail_ready
    nop
    mtc2 zero,$v06.e0
    mtc2 zero,$v06.e1
    mtc2 zero,$v06.e2
q3_tail_ready:
q3_below_ready:
    vmulf $v01,$v02,$v21.e0
    vmacf $v01,$v03,$v21.e0
    vmacf $v01,$v04,$v21.e0
    vadd $v01,$v01,$v05
    li t3,%lo(base_buffer)
    sqv $v01,16,t3
    addiu t4,t3,14
    lqv $v07,0,t4
    lrv $v07,16,t4
    addiu t4,t3,12
    lqv $v08,0,t4
    lrv $v08,16,t4
    addiu t4,t3,10
    lqv $v09,0,t4
    lrv $v09,16,t4
    vmulf $v10,$v07,$v21.e0
    vmadh $v10,$v01,K1
    vmacf $v10,$v08,$v21.e1
    vmacf $v10,$v09,$v21.e2
    vmacf $v10,$v06,$v21.e3
    sqv $v01,0,t3
    li t3,1
    beq v1,t3,q3_last_batch
    nop
    sqv $v10,0,a0
    srv $v10,16,a0
    b q3_stored
    nop
q3_last_batch:
    .set lane,0
    .rept 6
    ssv $v10,lane*2,lane*2,a0
    .set lane,lane+1
    .endr
q3_stored:
    addiu a0,a0,16
    addiu a1,a1,16
    addiu k1,k1,16
    addiu a2,a2,16
    addiu v1,v1,-1
    bnez v1,q3_batch
    nop
    lh t3,2(s6)
    lh t4,SHORT_ROW-4(s6)
    sh t3,0(s6)
    sh t4,SHORT_ROW-2(s6)
    li t3,PLASMAPONG_PRESSURE_PASSES-1
    bne s5,t3,q3_next_pass
    nop
    li t4,%lo(word_output)
    /* Expand eight signed Q3 values at a time. VMUDM yields the signed
       high half and ACC_LO the low half of the exact Q12 word. */
    .set part,0
    .rept WIDTH/8
    lqv $v09,part,s6
    vmudm $v10,$v09,$v21.e2
    vsar $v11,COP2_ACC_LO
    .set lane,0
    .rept 8
    ssv $v10,lane*2,lane*4,t4
    ssv $v11,lane*2,lane*4+2,t4
    .set lane,lane+1
    .endr
    addiu t4,t4,32
    .set part,part+16
    .endr
    move s0,s1
    li s4,%lo(word_output)
    li t0,WORD_ROW-1
    li t1,WORD_ROW
    jal DMAExec
    li t2,DMA_OUT
    li t3,1
    bne v0,t3,q3_store_bottom
    nop
    addiu s0,s1,-WORD_ROW
    li s4,%lo(word_output)
    jal DMAExec
    li t2,DMA_OUT
q3_store_bottom:
    li t3,HEIGHT-2
    bne v0,t3,q3_store_done
    nop
    addiu s0,s1,WORD_ROW
    li s4,%lo(word_output)
    jal DMAExec
    li t2,DMA_OUT
q3_store_done:
    lw s0,%lo(short_cursor)(zero)
    beqz s0,q3_short_stored
    nop
    move s4,s6
    li t0,SHORT_ROW-1
    li t1,SHORT_ROW
    jal DMAExec
    li t2,DMA_OUT
    li t3,1
    bne v0,t3,q3_short_bottom
    nop
    lw s0,%lo(short_cursor)(zero)
    addiu s0,s0,-SHORT_ROW
    move s4,s6
    li t0,SHORT_ROW-1
    li t1,SHORT_ROW
    jal DMAExec
    li t2,DMA_OUT
q3_short_bottom:
    li t3,HEIGHT-2
    bne v0,t3,q3_short_next
    nop
    lw s0,%lo(short_cursor)(zero)
    addiu s0,s0,SHORT_ROW
    move s4,s6
    li t0,SHORT_ROW-1
    li t1,SHORT_ROW
    jal DMAExec
    li t2,DMA_OUT
q3_short_next:
    lw t3,%lo(short_cursor)(zero)
    addiu t3,t3,SHORT_ROW
    sw t3,%lo(short_cursor)(zero)
q3_short_stored:
    addiu s1,s1,WORD_ROW
q3_next_pass:
    addiu s5,s5,1
    li t3,PLASMAPONG_PRESSURE_PASSES
    beq s5,t3,q3_next_tick
    nop
    addiu s6,s6,-SHORT_ROW
    li t3,%lo(rows)
    sltu t4,s6,t3
    beqz t4,q3_current_ready
    nop
    li s6,%lo(rows_end)-SHORT_ROW
q3_current_ready:
    addiu s7,s7,-SHORT_ROW
    li t3,%lo(divrows)
    sltu t4,s7,t3
    beqz t4,q3_pass
    nop
    b q3_pass
    li s7,%lo(divrows_end)-SHORT_ROW
q3_next_tick:
    addiu s3,s3,1
    li t3,(HEIGHT-2)+PLASMAPONG_PRESSURE_PASSES
    beq s3,t3,q3_done
    nop
    addiu fp,fp,SHORT_ROW
    li t3,%lo(rows_end)
    bne fp,t3,q3_pressure_cursor
    nop
    li fp,%lo(rows)
q3_pressure_cursor:
    addiu k0,k0,SHORT_ROW
    li t3,%lo(divrows_end)
    bne k0,t3,q3_tick
    nop
    b q3_tick
    li k0,%lo(divrows)
q3_done:
    j RSPQ_Loop
    nop

q3_load_warm:
    move v1,ra
    lw t3,%lo(warm_stride)(zero)
    li t4,SHORT_ROW
    bne t3,t4,q3_load_words
    nop
    move s0,t8
    move s4,t9
    li t0,SHORT_ROW-1
    li t1,SHORT_ROW
    jal DMAExec
    li t2,DMA_IN
    b q3_warm_walls
    nop
q3_load_words:
    move s0,t8
    li s4,%lo(div_input)
    li t0,WORD_ROW-1
    li t1,WORD_ROW
    jal DMAExec
    li t2,DMA_IN
    li t3,%lo(div_input)
    move t4,t9
    .set part,0
    .rept WIDTH/4
    lqv $v10,part,t3
    vmudh $v11,$v22,$v10.q0
    vmadm $v11,$v22,$v10.q1
    ssv $v11,0,0,t4
    ssv $v11,4,2,t4
    ssv $v11,8,4,t4
    ssv $v11,12,6,t4
    addiu t4,t4,8
    .set part,part+16
    .endr
q3_warm_walls:
    /* Enforce the Neumann sides even for an arbitrary caller's warm guess. */
    lh t5,2(t9)
    sh t5,0(t9)
    lh t5,SHORT_ROW-4(t9)
    sh t5,SHORT_ROW-2(t9)
    jr v1
    nop
