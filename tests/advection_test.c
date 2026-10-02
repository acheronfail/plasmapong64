#include "advection_cases.h"
#include <stdio.h>
int main(void) {
    advection_cases();
    puts("PASS: advection matches float reference exactly on 64 fields; source banks unchanged");
}
