#include "key.h"

/* =========================================================
 *                  按键初始化
 * ========================================================= */

void key_init(void)
{
    pinMode(KEY1, INPUT_PULLUP);

    pinMode(KEY2, INPUT_PULLUP);

    pinMode(KEY3, INPUT_PULLUP);
}