# MAYO side-channel analysis
This repository contains firmware, Jupyter notebooks, and supporting data used for side-channel analysis experiments on the MAYO post-quantum signature scheme. The experiments were performed using ChipWhisperer hardware and include trace capture, an unmasked CPA attack, and masked CPA attacks against two-share and three-share implementations.

The attack notebooks can be executed without capture hardware if the required trace datasets and peak files are available in the `data/` directory. However, recapturing traces or flashing firmware requires the correct ChipWhisperer setup and target hardware.

## MAYO version
The MAYO implementation included here is based on commit `29b1436` of:

https://github.com/PQCMayo/MAYO-M4.git

## Repository layout

```text
sca-mayo/
├── data/
├── firmware/
└── notebooks/
```

## Firmware
The firmware projects are based on ChipWhisperer firmware examples and communicate with the capture notebooks using the SimpleSerial interface. To build and flash the firmware, the firmware source directories must be placed inside the ChipWhisperer firmware tree:

chipwhisperer/firmware/mcu/

For example, the firmware directories should be available as:

```text
chipwhisperer/firmware/mcu/simpleserial-mayo1nsstm/
chipwhisperer/firmware/mcu/simpleserial-mayo1masked/
chipwhisperer/firmware/mcu/simpleserial-mayo1nssam4s/
```

The firmware must be built and flashed from a ChipWhisperer environment with access to the correct target hardware. The STM32F4 firmware is used for the main capture and attack notebooks, while the SAM4S firmware is used only for the experimental notebook.


## Firmware overview

| Firmware folder             | Target board  | Used by                                   |
| --------------------------- | ------------- | ------------------------------------------|
| `simpleserial-mayo1nsstm`   |    STM32F4    | Unmasked STM32F4 capture                  |
| `simpleserial-mayo1masked`  |    STM32F4    | Masked two-share and three-share capture  |
| `simpleserial-mayo1nssam4s` |    SAM4S      | Experimental SAM4S CPA notebook           |

## Notebooks

The repository contains four main notebooks. The first notebook is used for trace capture and peak detection for the supported STM32F4 MAYO configurations. The remaining three notebooks load pre-captured traces and peak indices from the `data/` directory and perform the corresponding attacks.

| Notebook | Description | Required data / firmware |
|---|---|---|
| `01_capture_mayo1_stm32f4.ipynb` | Captures power traces and identifies target-function call locations. This notebook can be used for unmasked, two-share masked, and three-share masked MAYO captures by selecting the corresponding firmware image and capture settings. | Requires ChipWhisperer Husky, STM32F4, and either `simpleserial-mayo1nsstm` or `simpleserial-mayo1masked`, depending on the selected capture. |
| `02_attack_mayo1_stm32f4_unmasked.ipynb` | Performs a CPA attack on the unmasked STM32F4 implementation. | Loads `data_raw.npy` and `peaks_unmasked.npy` from `data/`. `data_raw.npy` is included in `data/unmasked_and_2share_raw_traces.zip`. |
| `03_attack_mayo1_masked_2shares.ipynb` | Performs a two-share masked CPA attack. | Loads `masked_data_raw_trace.npy` and `masked_peaks_2shares.npy` from `data/`. `masked_data_raw_trace.npy` is included in `data/unmasked_and_2share_raw_traces.zip`. |
| `04_attack_mayo1_masked_3shares.ipynb` | Performs a three-share masked CPA attack. | Loads `3shares_masked_rawtrace.npy` and `3shares_peaks.npy` from `data/`. The peak file is included, but the raw trace is not stored directly in this repository due to file size. |

### Experimental notebook

The `notebooks/experiments/` directory contains initial experiments performed on a simplified SAM4S target:

```text
notebooks/experiments/sam4s_initial_cpa_experiments.ipynb
```

## Data

The repository includes a compressed trace archive:

```text
data/unmasked_and_2share_raw_traces.zip
```

This archive contains the raw traces needed to run the unmasked attack notebook and the two-share masked attack notebook:

```text
data_raw.npy
masked_data_raw_trace.npy
```

Extract the archive into the `data/` directory before running the attack notebooks:

```bash
cd data
unzip unmasked_and_2share_raw_traces.zip
```

After extraction, the following attack notebooks can be run directly from the included trace data:

| Notebook | Trace file | Peak file |
|---|---|---|
| `02_attack_mayo1_stm32f4_unmasked.ipynb` | `data_raw.npy` | `peaks_unmasked.npy` |
| `03_attack_mayo1_masked_2shares.ipynb` | `masked_data_raw_trace.npy` | `masked_peaks_2shares.npy` |


The file `data/data.py` contains MAYO key data used during the attacks.
