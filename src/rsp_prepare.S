/* Dye/velocity visualisation and divergence. RSPQ owns gp
 * and v00/v30/v31. Each command reloads scratch; buffers own cache lines. */
#include <rsp_queue.inc>
#include "fluid_config.h"
#define WIDTH PLASMAPONG_GRID_W
#define HEIGHT PLASMAPONG_GRID_H
#define ROW_BYTES (WIDTH*2)
#define WORD_ROW (WIDTH*4)
#define PLANE_BYTES (WIDTH*HEIGHT*2)
    .set noreorder
    .set at
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand Pixels,16
    RSPQ_DefineCommand Divergence,12
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
scratch: .ds.b (9*ROW_BYTES+1040)
    .set input0,scratch
    .set input1,scratch+(3*ROW_BYTES)
    .set input2,scratch+(6*ROW_BYTES)
    .set result,scratch+(7*ROW_BYTES)
    .set speed_palette,scratch+(9*ROW_BYTES)
    .text
.macro transfer base, buffer, size, direction
    move s0,\base
    li s4,%lo(\buffer)
    li t0,\size-1
    li t1,\size
    jal DMAExec
    li t2,\direction
.endm
Pixels16:
    b Pixels
    li a3,1
Pixels:
    move s1,a0
    move s2,a1
    move s3,a2
    li s5,HEIGHT
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
    transfer s1,input0,ROW_BYTES,DMA_IN
    addiu s7,s1,PLANE_BYTES
    transfer s7,input1,ROW_BYTES,DMA_IN
    addiu s7,s1,2*PLANE_BYTES
    transfer s7,input2,ROW_BYTES,DMA_IN
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
    li t3,1
    beq a3,t3,pixels_pack16
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
    slti t3,s6,ROW_BYTES
    bnez t3,pixels_batch
    nop
    li t3,1
    beq a3,t3,pixels_dma16
    nop
    transfer s2,result,WORD_ROW,DMA_OUT
    b pixels_dma_done
    nop
pixels_dma16:
    transfer s2,result,ROW_BYTES,DMA_OUT
pixels_dma_done:
    addiu s1,s1,ROW_BYTES
    addu s2,s2,s3
    addiu s5,s5,-1
    bnez s5,pixels_row
    nop
    j RSPQ_Loop
    nop
/* a0=u, a1=v, a2=divergence. Only interior rows are consumed by pressure. */
Divergence:
#ifdef PLASMAPONG_VELOCITY_CHAIN
    /* Bit 31 requests virtual zero wall normals for an RSP-owned input.
       Final gradient writes the physical walls; standalone callers use zero. */
    srl k0,a2,31
    sll a2,a2,1
    srl a2,a2,1
#endif
    addiu s1,a0,ROW_BYTES
    move s2,a1
    addiu s3,a2,WORD_ROW
    li s5,HEIGHT-2
    li t3,-(PLASMAPONG_CELL_Q4*8)
    mtc2 t3,$v20.e0
    ctc2 zero,COP2_CTRL_VCO
divergence_row:
    transfer s1,input0,ROW_BYTES,DMA_IN
    transfer s2,input1,ROW_BYTES,DMA_IN
    addiu s7,s2,WORD_ROW
    transfer s7,input2,ROW_BYTES,DMA_IN
#ifdef PLASMAPONG_VELOCITY_CHAIN
    vnand $v21,vzero,vzero
    vnand $v22,vzero,vzero
    beqz k0,divergence_walls_ready
    nop
    sh zero,%lo(input0)(zero)
    sh zero,%lo(input0)+ROW_BYTES-2(zero)
    li t3,HEIGHT-2
    bne s5,t3,divergence_not_top
    nop
    vxor $v21,vzero,vzero
divergence_not_top:
    li t3,1
    bne s5,t3,divergence_walls_ready
    nop
    vxor $v22,vzero,vzero
divergence_walls_ready:
#endif
    move s6,zero
divergence_batch:
    addiu t3,s6,%lo(input0)
    addiu t4,s6,%lo(input1)
    addiu t5,s6,%lo(input2)
    lqv $v01,0,t4
    lqv $v02,0,t5
#ifdef PLASMAPONG_VELOCITY_CHAIN
    vand $v01,$v01,$v21
    vand $v02,$v02,$v22
#endif
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
    slti t3,s6,ROW_BYTES
    bnez t3,divergence_batch
    nop
    sw zero,%lo(result)(zero)
    sw zero,%lo(result)+WORD_ROW-4(zero)
    transfer s3,result,WORD_ROW,DMA_OUT
    addiu s1,s1,ROW_BYTES
    addiu s2,s2,ROW_BYTES
    addiu s3,s3,WORD_ROW
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
SpeedPixels16:
    ori a2,a2,0x8000
SpeedPixels:
    move s1,a0
    move s2,a1
    andi s3,a2,0x7fff
    li s5,HEIGHT
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
    transfer s1,input0,ROW_BYTES,DMA_IN
    addiu s7,s1,PLANE_BYTES
    transfer s7,input1,ROW_BYTES,DMA_IN
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
    slti t3,s6,ROW_BYTES
    bnez t3,speed_batch
    nop
    andi t6,a2,0x8000
    bnez t6,speed_dma16
    nop
    transfer s2,result,WORD_ROW,DMA_OUT
    b speed_dma_done
    nop
speed_dma16:
    transfer s2,result,ROW_BYTES,DMA_OUT
speed_dma_done:
    addiu s1,s1,ROW_BYTES
    addu s2,s2,s3
    addiu s5,s5,-1
    bnez s5,speed_row
    nop
    j RSPQ_Loop
    nop
