/* Exact CPU-equivalent backtraces, RGBA32 dye conversion and divergence. RSPQ owns gp
 * and v00/v30/v31. Each command reloads scratch; buffers own cache lines. */
#include <rsp_queue.inc>
    .set noreorder
    .set at
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand Trace,16
    RSPQ_DefineCommand Pixels,16
    RSPQ_DefineCommand Divergence,12
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
input0: .ds.b 96
input1: .ds.b 96
input2: .ds.b 96
result: .ds.b 288
    .text
.macro transfer base, buffer, size, direction
    move s0,\base
    li s4,%lo(\buffer)
    li t0,\size-1
    li t1,\size
    jal DMAExec
    li t2,\direction
.endm
/* a0=u, a1=v, a2=trace, a3=Q20 timestep.
 * x*4096 - ((u*step+2048)>>12) ==
 * x*4096 + ((-u*step+2047)>>12). Split the latter product into
 * signed integer cells and unsigned Q12 fractions before clamping. */
Trace:
    move s1,a0
    move s2,a1
    move s3,a2
    mtc2 a3,$v20.e0
    vxor $v20,vzero,$v20.e0
    srl t3,a3,16
    mtc2 t3,$v21.e0
    li t3,2047
    mtc2 t3,$v22.e0
    vxor $v22,vzero,$v22.e0
    li t3,256
    mtc2 t3,$v23.e0
    li t3,16
    mtc2 t3,$v23.e1
    li t3,4095
    mtc2 t3,$v23.e2
    li t3,8
    mtc2 t3,$v23.e3
    li t3,32728             /* upper boundary fraction: (4096-5)*8 */
    mtc2 t3,$v23.e4
    li t3,96
    mtc2 t3,$v23.e5
    li t3,2
    mtc2 t3,$v23.e6
    li s5,0
    ctc2 zero,COP2_CTRL_VCO
trace_row:
    transfer s1,input0,96,DMA_IN
    transfer s2,input1,96,DMA_IN
    move s6,zero
    mtc2 s5,$v25.e0
    vxor $v25,vzero,$v25.e0
    /* Eight adjacent x coordinates, incremented after each batch. */
    li t3,0
    mtc2 t3,$v24.e0
    li t3,1
    mtc2 t3,$v24.e1
    li t3,2
    mtc2 t3,$v24.e2
    li t3,3
    mtc2 t3,$v24.e3
    li t3,4
    mtc2 t3,$v24.e4
    li t3,5
    mtc2 t3,$v24.e5
    li t3,6
    mtc2 t3,$v24.e6
    li t3,7
    mtc2 t3,$v24.e7
trace_batch:
    sll t3,s6,1
    addiu t4,t3,%lo(input0)
    addiu t5,t3,%lo(input1)
    lqv $v01,0,t4
    lqv $v02,0,t5
.macro coordinate source, origin, bound, cell, frac
    vsub $v03,vzero,\source
    vmudn $v04,$v20,$v03
    vmadh $v04,$v03,$v21.e0
    vmadn $v04,$v22,K1
    vsar $v05,COP2_ACC_MD
    vsar $v06,COP2_ACC_LO
    vmudm \cell,$v05,$v23.e0
    vadd \cell,\cell,\origin
    vmudn $v05,$v05,$v23.e1
    vmudl $v06,$v06,$v23.e1
    vor \frac,$v05,$v06
    vand \frac,\frac,$v23.e2
    vmudn \frac,\frac,$v23.e3
    vlt $v07,\cell,vzero
    vmrg \frac,vzero,\frac
    vge \cell,\cell,vzero
    li t3,\bound
    mtc2 t3,$v26.e0
    addiu t3,t3,-1
    mtc2 t3,$v26.e1
    /* The last valid cell also needs its fraction capped at 4091. */
    vlt $v28,\frac,$v23.e4
    vge $v07,\cell,$v26.e1
    vmrg \frac,$v28,\frac
    vge $v07,\cell,$v26.e0
    vxor $v27,vzero,$v23.e4
    vmrg \frac,$v27,\frac
    vxor $v27,vzero,$v26.e1
    vmrg \cell,$v27,\cell
.endm
    coordinate $v01,$v24,47,$v08,$v09
    coordinate $v02,$v25,32,$v10,$v11
    vmudn $v10,$v10,$v23.e5
    vmudn $v08,$v08,$v23.e6
    vadd $v08,$v08,$v10
    sll t3,s6,1
    addu t3,t3,s6
    sll t3,t3,1
    addiu t3,t3,%lo(result)
    .set lane,0
    .rept 8
    ssv $v08,lane*2,lane*6,t3
    ssv $v09,lane*2,lane*6+2,t3
    ssv $v11,lane*2,lane*6+4,t3
    .set lane,lane+1
    .endr
    vadd $v24,$v24,$v23.e3
    addiu s6,s6,8
    slti t3,s6,48
    bnez t3,trace_batch
    nop
    transfer s3,result,288,DMA_OUT
    addiu s1,s1,96
    addiu s2,s2,96
    addiu s3,s3,288
    addiu s5,s5,1
    slti t3,s5,33
    bnez t3,trace_row
    nop
    j RSPQ_Loop
    nop
/* a0=three planar dye channels, a1=RGBA32, a2=row byte stride, a3=unused. */
Pixels:
    move s1,a0
    move s2,a1
    move s3,a2
    li s5,33
    li t3,255
    mtc2 t3,$v20.e0
    li t3,256
    mtc2 t3,$v21.e0
    li t3,1680
    mtc2 t3,$v10.e0
    li t3,160
    mtc2 t3,$v10.e1
    li t3,2040
    mtc2 t3,$v10.e2
    li t3,5
    mtc2 t3,$v13.e0
    vxor $v13,vzero,$v13.e0
    li t3,512
    mtc2 t3,$v11.e0
    li t3,1240
    mtc2 t3,$v11.e1
    li t3,1640
    mtc2 t3,$v11.e2
    li t3,9
    mtc2 t3,$v14.e0
    vxor $v14,vzero,$v14.e0
    li t3,736
    mtc2 t3,$v12.e0
    li t3,1800
    mtc2 t3,$v12.e1
    li t3,200
    mtc2 t3,$v12.e2
    li t3,22
    mtc2 t3,$v15.e0
    vxor $v15,vzero,$v15.e0
    ctc2 zero,COP2_CTRL_VCO
pixels_row:
    transfer s1,input0,96,DMA_IN
    addiu s7,s1,3168
    transfer s7,input1,96,DMA_IN
    addiu s7,s1,6336
    transfer s7,input2,96,DMA_IN
    move s6,zero
pixels_batch:
    addiu t3,s6,%lo(input0)
    addiu t4,s6,%lo(input1)
    addiu t5,s6,%lo(input2)
    lqv $v01,0,t3
    lqv $v02,0,t4
    lqv $v03,0,t5
.macro component dest, r, b, g, base
    vmudm \dest,$v01,\r
    vmadm \dest,$v02,\b
    vmadm \dest,$v03,\g
    vmadh \dest,\base,K1
    vlt \dest,\dest,$v20.e0
.endm
    component $v04,$v10.e0,$v10.e1,$v10.e2,$v13
    component $v05,$v11.e0,$v11.e1,$v11.e2,$v14
    component $v06,$v12.e0,$v12.e1,$v12.e2,$v15
    vmudn $v04,$v04,$v21.e0
    vmudn $v06,$v06,$v21.e0
    vor $v04,$v04,$v05
    vor $v06,$v06,$v20.e0
    sll t3,s6,1
    addiu t3,t3,%lo(result)
    .set lane,0
    .rept 8
    ssv $v04,lane*2,lane*4,t3
    ssv $v06,lane*2,lane*4+2,t3
    .set lane,lane+1
    .endr
    addiu s6,s6,16
    slti t3,s6,96
    bnez t3,pixels_batch
    nop
    transfer s2,result,192,DMA_OUT
    addiu s1,s1,96
    addu s2,s2,s3
    addiu s5,s5,-1
    bnez s5,pixels_row
    nop
    j RSPQ_Loop
    nop
/* a0=u, a1=v, a2=divergence. Only interior rows are consumed by pressure. */
Divergence:
    addiu s1,a0,96
    move s2,a1
    addiu s3,a2,192
    li s5,31
    li t3,-768
    mtc2 t3,$v20.e0
    ctc2 zero,COP2_CTRL_VCO
divergence_row:
    transfer s1,input0,96,DMA_IN
    transfer s2,input1,96,DMA_IN
    addiu s7,s2,192
    transfer s7,input2,96,DMA_IN
    move s6,zero
divergence_batch:
    addiu t3,s6,%lo(input0)
    addiu t4,s6,%lo(input1)
    addiu t5,s6,%lo(input2)
    lqv $v01,0,t4
    lqv $v02,0,t5
    .set lane,0
    .rept 8
    lsv $v03,lane*2,lane*2-2,t3
    lsv $v04,lane*2,lane*2+2,t3
    .set lane,lane+1
    .endr
    vsub $v02,$v02,$v01
    vsub $v04,$v04,$v03
    vmudh $v05,$v02,$v20.e0
    vmadh $v05,$v04,$v20.e0
    vsar $v05,COP2_ACC_HI
    vsar $v06,COP2_ACC_MD
    sll t3,s6,1
    addiu t3,t3,%lo(result)
    .set lane,0
    .rept 8
    ssv $v05,lane*2,lane*4,t3
    ssv $v06,lane*2,lane*4+2,t3
    .set lane,lane+1
    .endr
    addiu s6,s6,16
    slti t3,s6,96
    bnez t3,divergence_batch
    nop
    sw zero,%lo(result)(zero)
    sw zero,%lo(result)+188(zero)
    transfer s3,result,192,DMA_OUT
    addiu s1,s1,96
    addiu s2,s2,96
    addiu s3,s3,192
    addiu s5,s5,-1
    bnez s5,divergence_row
    nop
    j RSPQ_Loop
    nop
