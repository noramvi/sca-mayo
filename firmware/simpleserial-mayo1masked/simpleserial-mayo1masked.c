#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hal.h"
#include "simpleserial.h"
#include "p1p1t_data.h"
#include "api.h"
#include "arithmetic.h"
#include "arithmetic_fixed.h"
#include "generic_arithmetic.h"
#include "stm32f4_hal.h"
#include "mayo.h"
#include "rng.h"

#define V 78
#define OIL 8
#define LIMBS 5
#define N_SHARES 3

#if N_SHARES != 2 && N_SHARES != 3
#error "N_SHARES must be 2 or 3"
#endif

static uint8_t  O_global[V * OIL];

static uint64_t acc_init_full_buffer[V * OIL * LIMBS];
static uint8_t  O_shares[N_SHARES][V * OIL];
static uint64_t acc_shares[N_SHARES][V * OIL * LIMBS];

static uint32_t O_offset = 0;
static uint32_t p2_full_offset = 0;
static size_t acc_full_read_offset = 0;


static int share_O_from_global(void)
{
    uint8_t rnd[V * OIL];

    memset(O_shares, 0, sizeof(O_shares));

    for (int s = 1; s < N_SHARES; s++) {
        if (randombytes(rnd, sizeof(rnd)) != 0) {
            return -1;
        }

        for (size_t i = 0; i < sizeof(O_global); i++) {
            O_shares[s][i] = rnd[i] & 0x0F;
        }
    }

    for (size_t i = 0; i < sizeof(O_global); i++) {
        uint8_t x = O_global[i] & 0x0F;

        for (int s = 1; s < N_SHARES; s++) {
            x ^= O_shares[s][i];
        }

        O_shares[0][i] = x & 0x0F;
    }

    return 0;
}

static void masked_p1p1t_times_O(void)
{
    const int param_v = V;
    const int param_o = OIL;
    const int m_vec_limbs = LIMBS;

    const uint64_t *P1 = p1_data;

    for (int s = 0; s < N_SHARES; s++) {
        int bs_mat_entries_used = 0;

        for (int r = 0; r < param_v; r++) {
            for (int c = r; c < param_v; c++) {
                if (c == r) {
                    bs_mat_entries_used += 1;
                    continue;
                }

                const uint64_t *P1_entry =
                    P1 + m_vec_limbs * bs_mat_entries_used;

                for (int k = 0; k < param_o; k++) {
                    m_vec_mul_add(
                        m_vec_limbs,
                        P1_entry,
                        O_shares[s][c * param_o + k],
                        acc_shares[s] + m_vec_limbs * (r * param_o + k)
                    );

                    m_vec_mul_add(
                        m_vec_limbs,
                        P1_entry,
                        O_shares[s][r * param_o + k],
                        acc_shares[s] + m_vec_limbs * (c * param_o + k)
                    );
                }

                bs_mat_entries_used += 1;
            }
        }
    }
}


uint8_t test_cmd(uint8_t cmd, uint8_t scmd,
                 uint8_t len, uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)buf;
    (void)len;

    trigger_high();
    for (volatile uint32_t i = 0; i < 200000; i++);
    trigger_low();

    uint8_t ok = 0x99;
    simpleserial_put('r', 1, &ok);

    return SS_ERR_OK;
}


uint8_t load_O(uint8_t cmd,
                   uint8_t scmd,
                   uint8_t len,
                   uint8_t *buf)
{
    (void)cmd;
    (void)scmd;

    if (O_offset == 0) {
        memset(O_global, 0, sizeof(O_global));
    }

    memcpy(O_global + O_offset, buf, len);
    O_offset += len;

    if (O_offset >= sizeof(O_global)) {
        O_offset = 0;
    }

    return SS_ERR_OK;
}


uint8_t load_P2(uint8_t cmd,
                    uint8_t scmd,
                    uint8_t len,
                    uint8_t *buf)
{
    (void)cmd;
    (void)scmd;

    if (p2_full_offset == 0) {
        memset(acc_init_full_buffer, 0, sizeof(acc_init_full_buffer));
    }

    memcpy((uint8_t*)acc_init_full_buffer + p2_full_offset, buf, len);
    p2_full_offset += len;

    if (p2_full_offset >= sizeof(acc_init_full_buffer)) {
        p2_full_offset = 0;
    }

    return SS_ERR_OK;
}

static void recombine_acc_shares_to_share0(void)
{
    const size_t acc_words = V * OIL * LIMBS;

    for (int s = 1; s < N_SHARES; s++) {
        for (size_t i = 0; i < acc_words; i++) {
            acc_shares[0][i] ^= acc_shares[s][i];
        }
    }
}


uint8_t run_full_p1p1t_masked(uint8_t cmd,
                                  uint8_t scmd,
                                  uint8_t len,
                                  uint8_t *buf)
{
    (void)cmd;
    (void)scmd;
    (void)len;
    (void)buf;

    if (share_O_from_global() != 0) {
        uint8_t err = 0xEE;
        simpleserial_put('r', 1, &err);
        return SS_ERR_OK;
    }

    memset(acc_shares, 0, sizeof(acc_shares));
    memcpy(acc_shares[0], acc_init_full_buffer, sizeof(acc_init_full_buffer));

    acc_full_read_offset = 0;

    trigger_high();

    masked_p1p1t_times_O();

    trigger_low();

    recombine_acc_shares_to_share0();

    uint8_t ok = 0xA5;
    simpleserial_put('r', 1, &ok);

    return SS_ERR_OK;
}




uint8_t read_full_acc(uint8_t cmd,
                          uint8_t scmd,
                          uint8_t len,
                          uint8_t *buf)
{
    (void)cmd;
    (void)scmd;

    if (len < 1) {
        return SS_ERR_OK;
    }

    uint8_t out_len = buf[0];

    size_t remaining = sizeof(acc_shares[0]) - acc_full_read_offset;
    if (out_len > remaining) {
        out_len = (uint8_t)remaining;
    }

    simpleserial_put(
        'r',
        out_len,
        ((uint8_t*)acc_shares[0]) + acc_full_read_offset
    );

    acc_full_read_offset += out_len;

    if (acc_full_read_offset >= sizeof(acc_shares[0])) {
        acc_full_read_offset = 0;
    }

    return SS_ERR_OK;
}


int main(void)
{
    platform_init();
    init_uart();
    trigger_setup();

    simpleserial_init();

    simpleserial_addcmd('t', 0, test_cmd);
    simpleserial_addcmd('o', 0, load_O);
    simpleserial_addcmd('i', 0, load_P2);
    simpleserial_addcmd('g', 1, read_full_acc);
    simpleserial_addcmd('a', 0, run_full_p1p1t_masked);

    while (1) {
        simpleserial_get();
    }
}