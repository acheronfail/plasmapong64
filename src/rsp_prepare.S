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
    RSPQ_DefineCommand Gradient,12
    RSPQ_DefineCommand SpeedPixels,16
    RSPQ_DefineCommand Pixels16,16
    RSPQ_DefineCommand SpeedPixels16,16
RSPQ_EndOverlayHeader
    /* Embedded data can be only 8-byte aligned in RDRAM. Keep the command
       table's CPU cache line separate from the state saved by RSP DMA. */
    .align 4
    .space 16
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
/* Commands execute serially and reload all consumed scratch. Alias their
   different layouts instead of reserving every command's buffers together. */
#ifndef PLASMAPONG_TRACE_ROWS
#define PLASMAPONG_TRACE_ROWS 3
#endif
#define TRACE_ROWS PLASMAPONG_TRACE_ROWS
scratch: .ds.b 1904
    .set input0,scratch
    .set input1,scratch+288
    .set input2,scratch+576
    .set result,scratch+672
    .set grad_above,scratch
    .set grad_middle,scratch+192
    .set grad_below,scratch+384
    .set grad_u,scratch+576
    .set grad_v,scratch+672
    .set speed_palette,scratch+864
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
    /* Retain initial x coordinates in Trace's otherwise unused third input.
       This command reloads them after every overlay/command transition. */
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
    li t3,%lo(input2)
    sqv $v24,0,t3
trace_chunk:
    /* Share each DMA setup across rows; the selected count divides 33. */
    transfer s1,input0,96*TRACE_ROWS,DMA_IN
    transfer s2,input1,96*TRACE_ROWS,DMA_IN
    move s7,zero
trace_row:
    move s6,zero
    mtc2 s5,$v25.e0
    vxor $v25,vzero,$v25.e0
    li t3,%lo(input2)
    lqv $v24,0,t3
trace_batch:
    sll t3,s6,1
    addu t3,t3,s7
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
    addu t3,t3,s7
    sll t4,s7,1
    addu t3,t3,t4
    addiu t3,t3,%lo(result)
    /* Planar eight-cell trace blocks avoid scalar interleaving. */
    sqv $v08,0,t3
    sqv $v09,16,t3
    sqv $v11,32,t3
    vadd $v24,$v24,$v23.e3
    addiu s6,s6,8
    slti t3,s6,48
    bnez t3,trace_batch
    nop
    addiu s5,s5,1
    addiu s7,s7,96
    slti t3,s7,96*TRACE_ROWS
    bnez t3,trace_row
    nop
    transfer s3,result,288*TRACE_ROWS,DMA_OUT
    addiu s1,s1,96*TRACE_ROWS
    addiu s2,s2,96*TRACE_ROWS
    addiu s3,s3,288*TRACE_ROWS
    slti t3,s5,33
    bnez t3,trace_chunk
    nop
    j RSPQ_Loop
    nop
/* a0=three planar dye channels, a1=RGBA32, a2=row byte stride, a3=unused. */
Pixels16:
    li a3,1
Pixels:
    move s1,a0
    move s2,a1
    move s3,a2
    li s5,33
    li t3,255
    mtc2 t3,$v20.e0
    li t3,256
    mtc2 t3,$v21.e0
    li t3,8192
    mtc2 t3,$v22.e0
    li t3,2048
    mtc2 t3,$v22.e1
    li t3,64
    mtc2 t3,$v22.e2
    li t3,2
    mtc2 t3,$v22.e3
    li t3,1
    mtc2 t3,$v22.e4
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
    bnez a3,pixels_pack16
    nop
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
    b pixels_next
    nop
pixels_pack16:
    /* Truncate each already-clamped 8-bit component to five bits. */
    vmudm $v04,$v04,$v22.e0
    vmudm $v05,$v05,$v22.e0
    vmudm $v06,$v06,$v22.e0
    vmudn $v04,$v04,$v22.e1
    vmudn $v05,$v05,$v22.e2
    vmudn $v06,$v06,$v22.e3
    vor $v04,$v04,$v05
    vor $v04,$v04,$v06
    vor $v04,$v04,$v22.e4
    addiu t3,s6,%lo(result)
    sqv $v04,0,t3
pixels_next:
    addiu s6,s6,16
    slti t3,s6,96
    bnez t3,pixels_batch
    nop
    bnez a3,pixels_dma16
    nop
    transfer s2,result,192,DMA_OUT
    b pixels_dma_done
    nop
pixels_dma16:
    transfer s2,result,96,DMA_OUT
pixels_dma_done:
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
    addiu t6,t3,-2
    lqv $v03,0,t6
    lrv $v03,16,t6
    addiu t6,t3,2
    lqv $v04,0,t6
    lrv $v04,16,t6
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
/* a0=u, a1=v, a2=Q12 pressure. Exact signed rounded division by 3072.
 * Let n=(abs(difference)+1536)>>10. For n<98304,
 * floor(n/3) == floor(n*43691/131072). Larger n can saturate to 32767:
 * subtracting that from any legal Q4 velocity already reaches the final clamp.
 * Unsigned halves retain all 32 difference bits; no float reciprocal is used. */
Gradient:
    move s1,a0
    move s2,a1
    move s3,a2
    li s5,0
    li t3,1536
    mtc2 t3,$v20.e0
    li t3,64
    mtc2 t3,$v20.e1
    li t3,43691
    mtc2 t3,$v20.e2
    li t3,32768
    mtc2 t3,$v20.e3
    li t3,1024
    mtc2 t3,$v20.e4
    li t3,32767
    mtc2 t3,$v20.e5
    li t3,-16383
    mtc2 t3,$v20.e6
    li t3,16383
    mtc2 t3,$v20.e7
    vnor $v21,vzero,vzero
    ctc2 zero,COP2_CTRL_VCO
    li fp,%lo(grad_above)
    li k0,%lo(grad_middle)
    li k1,%lo(grad_below)
    transfer s3,grad_middle,192,DMA_IN
gradient_row:
    /* Retain above/current rows: each pressure row is fetched only once.
       The unused above/below scratch on normal walls is masked on output. */
    li t3,32
    beq s5,t3,gradient_loaded
    nop
    addiu s0,s3,192
    move s4,k1
    li t0,191
    li t1,192
    jal DMAExec
    li t2,DMA_IN
gradient_loaded:
    transfer s1,grad_u,96,DMA_IN
    transfer s2,grad_v,96,DMA_IN
    move s6,zero
gradient_batch:
    sll t3,s6,1
    addu t3,t3,k0
    .set lane,0
    .rept 8
    lsv $v01,lane*2,lane*4-4,t3
    lsv $v02,lane*2,lane*4-2,t3
    lsv $v03,lane*2,lane*4+4,t3
    lsv $v04,lane*2,lane*4+6,t3
    .set lane,lane+1
    .endr
    addiu t4,s6,%lo(grad_u)
.macro gradient_gather enabled
    .if \enabled
    .if (gradient_lane % 4)==0
    lsv $v11,(gradient_lane/4)*2,(gradient_lane/4)*4,t5
    .elseif (gradient_lane % 4)==1
    lsv $v12,(gradient_lane/4)*2,(gradient_lane/4)*4+2,t5
    .elseif (gradient_lane % 4)==2
    lsv $v13,(gradient_lane/4)*2,(gradient_lane/4)*4,t6
    .else
    lsv $v14,(gradient_lane/4)*2,(gradient_lane/4)*4+2,t6
    .endif
    .set gradient_lane,gradient_lane+1
    .endif
.endm
.macro gradient_axis left_hi,left_lo,right_hi,right_lo,prefetch=0
    .set gradient_lane,0
    /* Difference v03:v04 - v01:v02. Only the low half wraps. */
    vsubc \right_lo,\right_lo,\left_lo
    gradient_gather \prefetch
    vsub \right_hi,\right_hi,\left_hi
    gradient_gather \prefetch
    vlt $v05,\right_hi,vzero
    gradient_gather \prefetch
    vmrg $v06,$v21,vzero
    gradient_gather \prefetch
    vand $v07,$v06,K1
    gradient_gather \prefetch
    vxor \right_hi,\right_hi,$v06
    gradient_gather \prefetch
    vxor \right_lo,\right_lo,$v06
    gradient_gather \prefetch
    vaddc \right_lo,\right_lo,$v07
    gradient_gather \prefetch
    vadd \right_hi,\right_hi,vzero
    gradient_gather \prefetch
    vor $v06,$v06,K1        /* restore sign later with +/-1 */
    gradient_gather \prefetch
    vaddc \right_lo,\right_lo,$v20.e0
    gradient_gather \prefetch
    vadd \right_hi,\right_hi,vzero
    gradient_gather \prefetch
    /* Low 16 bits of n; the 17th bit is high>=1024. */
    vmudn $v08,\right_hi,$v20.e1
    gradient_gather \prefetch
    vmudl $v09,\right_lo,$v20.e1
    gradient_gather \prefetch
    vor $v08,$v08,$v09
    gradient_gather \prefetch
    vmudl $v08,$v08,$v20.e2
    gradient_gather \prefetch
    vge $v05,\right_hi,$v20.e4
    gradient_gather \prefetch
    vxor $v09,vzero,$v20.e2
    gradient_gather \prefetch
    vmrg $v09,$v09,vzero
    gradient_gather \prefetch
    vaddc $v08,$v08,$v09
    gradient_gather \prefetch
    vmudl $v08,$v08,$v20.e3
    gradient_gather \prefetch
    ctc2 zero,COP2_CTRL_VCO
    gradient_gather \prefetch
    /* high>=1536 means n>=98304 and a guaranteed saturated result. */
    vge $v05,\right_hi,$v20.e0
    gradient_gather \prefetch
    vxor $v09,vzero,$v20.e5
    gradient_gather \prefetch
    vmrg $v08,$v09,$v08
    gradient_gather \prefetch
    vabs $v08,$v06,$v08
    gradient_gather \prefetch
    lqv $v10,0,t4
    gradient_gather \prefetch
    vsub $v10,$v10,$v08
    gradient_gather \prefetch
    vge $v10,$v10,$v20.e6
    gradient_gather \prefetch
    vlt $v10,$v10,$v20.e7
    gradient_gather \prefetch
    sqv $v10,0,t4
    gradient_gather \prefetch
    gradient_gather \prefetch
.endm
#ifdef PLASMAPONG_GRADIENT_PIPELINE
    /* Fill the independent vertical source bank during horizontal arithmetic.
       Vector loads use the scalar pipe; accumulator/control order is unchanged. */
    sll t3,s6,1
    addu t5,t3,fp
    addu t6,t3,k1
    gradient_axis $v01,$v02,$v03,$v04,1
    addiu t4,s6,%lo(grad_v)
    gradient_axis $v11,$v12,$v13,$v14
#else

    gradient_axis $v01,$v02,$v03,$v04
    sll t3,s6,1
    addu t4,t3,k1
    addu t3,t3,fp
    .set lane,0
    .rept 8
    lsv $v01,lane*2,lane*4,t3
    lsv $v02,lane*2,lane*4+2,t3
    lsv $v03,lane*2,lane*4,t4
    lsv $v04,lane*2,lane*4+2,t4
    .set lane,lane+1
    .endr
    addiu t4,s6,%lo(grad_v)
    gradient_axis $v01,$v02,$v03,$v04
#endif
    addiu s6,s6,16
    slti t3,s6,96
    bnez t3,gradient_batch
    nop
    sh zero,%lo(grad_u)(zero)
    sh zero,%lo(grad_u)+94(zero)
    beqz s5,gradient_zero_v
    li t3,32
    bne s5,t3,gradient_store
    nop
gradient_zero_v:
    li t3,%lo(grad_v)
    sqv vzero,0,t3
    sqv vzero,16,t3
    sqv vzero,32,t3
    sqv vzero,48,t3
    sqv vzero,64,t3
    sqv vzero,80,t3
gradient_store:
    transfer s1,grad_u,96,DMA_OUT
    transfer s2,grad_v,96,DMA_OUT
    move t3,fp
    move fp,k0
    move k0,k1
    move k1,t3
    addiu s1,s1,96
    addiu s2,s2,96
    addiu s3,s3,192
    addiu s5,s5,1
    slti t3,s5,33
    bnez t3,gradient_row
    nop
    j RSPQ_Loop
    nop
/* a0=velocity (u,v), a1=RGBA32 pixels, a2=row stride, a3=palette.
 * Exact integer max-plus-half-min magnitude and the shared CPU palette. */
SpeedPixels16:
    ori a2,a2,0x8000
SpeedPixels:
    move s1,a0
    move s2,a1
    andi s3,a2,0x7fff
    li s5,33
    transfer a3,speed_palette,1040,DMA_IN
    li t3,32768
    mtc2 t3,$v20.e0
    li t3,4096
    mtc2 t3,$v20.e1
    li t3,256
    mtc2 t3,$v20.e2
    li t3,4
    mtc2 t3,$v20.e3
    ctc2 zero,COP2_CTRL_VCO
speed_row:
    transfer s1,input0,96,DMA_IN
    addiu s7,s1,3168
    transfer s7,input1,96,DMA_IN
    move s6,zero
speed_batch:
    addiu t3,s6,%lo(input0)
    addiu t4,s6,%lo(input1)
    lqv $v01,0,t3
    lqv $v02,0,t4
    vabs $v01,$v01,$v01
    vabs $v02,$v02,$v02
    vlt $v03,$v01,$v02
    vge $v04,$v01,$v02
    vmudm $v03,$v03,$v20.e0
    vadd $v04,$v04,$v03
    vmudm $v04,$v04,$v20.e1
    vlt $v04,$v04,$v20.e2
    vmudn $v04,$v04,$v20.e3
    li t3,%lo(input2)
    sqv $v04,0,t3
    andi t6,a2,0x8000
    bnez t6,speed_pack16
    nop
    sll t4,s6,1
    addiu t4,t4,%lo(result)
    .set lane,0
    .rept 8
    lhu t5,lane*2(t3)
    addiu t5,t5,%lo(speed_palette)
    lw t5,0(t5)
    sw t5,lane*4(t4)
    .set lane,lane+1
    .endr
    b speed_next
    nop
speed_pack16:
    addiu t4,s6,%lo(result)
    .set lane,0
    .rept 8
    lhu t5,lane*2(t3)
    addiu t5,t5,%lo(speed_palette)
    lw t5,0(t5)
    sh t5,lane*2(t4)
    .set lane,lane+1
    .endr
speed_next:
    addiu s6,s6,16
    slti t3,s6,96
    bnez t3,speed_batch
    nop
    andi t6,a2,0x8000
    bnez t6,speed_dma16
    nop
    transfer s2,result,192,DMA_OUT
    b speed_dma_done
    nop
speed_dma16:
    transfer s2,result,96,DMA_OUT
speed_dma_done:
    addiu s1,s1,96
    addu s2,s2,s3
    addiu s5,s5,-1
    bnez s5,speed_row
    nop
    j RSPQ_Loop
    nop
