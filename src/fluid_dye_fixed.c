#include "fluid_dye_fixed.h"
#include <assert.h>
unsigned fluid_dye_decay(float decay) {
    assert(decay>=0 && decay<=1);
    return (unsigned)(decay*DYE_WEIGHT_SCALE+.5f);
}
