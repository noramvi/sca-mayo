#if defined(STM32F4)

#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_rng.h"

static RNG_HandleTypeDef hrng;
static int rng_inited = 0;

static int rng_init_once(void)
{
    if (rng_inited) return 0;
    __HAL_RCC_RNG_CLK_ENABLE();
    hrng.Instance = RNG;
    if (HAL_RNG_Init(&hrng) != HAL_OK) return -1;
    rng_inited = 1;
    return 0;
}

int PQCLEAN_randombytes(uint8_t *obuf, size_t len)
{
    if (rng_init_once() != 0) return -1;

    while (len) {
        uint32_t r;
        if (HAL_RNG_GenerateRandomNumber(&hrng, &r) != HAL_OK) return -1;

        size_t n = (len < 4) ? len : 4;
        for (size_t i = 0; i < n; i++) {
            *obuf++ = (uint8_t)(r >> (8 * i));
        }
        len -= n;
    }
    return 0;
}

int randombytes(uint8_t *obuf, size_t len) { return PQCLEAN_randombytes(obuf, len); }

#endif
