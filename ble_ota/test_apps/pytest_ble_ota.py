# Runs every Unity test case of the app. CI: QEMU. Locally: a connected board.
import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32c6'], indirect=['target'])
def test_ble_ota(dut: IdfDut) -> None:
    dut.run_all_single_board_cases()


@pytest.mark.qemu
@idf_parametrize('target', ['esp32c3'], indirect=['target'])
def test_ble_ota_qemu(dut: IdfDut) -> None:
    dut.run_all_single_board_cases()
