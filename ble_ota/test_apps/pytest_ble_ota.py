# Runs every Unity test case of the app. CI: QEMU. Locally: a connected board.
import subprocess
from pathlib import Path

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


def run_all_cases(dut: IdfDut) -> None:
    dut.run_all_single_board_cases(group='!coverage')
    # Writes the .gcda files next to the objects of the build, for gcovr.
    dut.write('[coverage]')
    stream = dut.expect(rb'gcov start\s+([0-9a-f]*)\s+gcov end').group(1)
    for gcda in Path(dut.app.binary_path).rglob('*.gcda'):
        gcda.unlink()
    subprocess.run(['riscv32-esp-elf-gcov-tool', 'merge-stream'], input=bytes.fromhex(stream.decode()), check=True)


@pytest.mark.generic
@idf_parametrize('target', ['esp32c6'], indirect=['target'])
def test_ble_ota(dut: IdfDut) -> None:
    run_all_cases(dut)


@pytest.mark.qemu
@idf_parametrize('target', ['esp32c3'], indirect=['target'])
def test_ble_ota_qemu(dut: IdfDut) -> None:
    run_all_cases(dut)
