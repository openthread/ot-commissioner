#!/usr/bin/env python3
"""
Unit tests for the qrcreds script.
"""

import unittest
import importlib.machinery
import importlib.util
import sys
import os
import contextlib
from io import StringIO


# Helper to import the standalone script without .py extension
def import_script(path):
    loader = importlib.machinery.SourceFileLoader('qrcreds', path)
    spec = importlib.util.spec_from_loader('qrcreds', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


script_path = os.path.join(os.path.dirname(__file__), 'qrcreds')
qrcreds = import_script(script_path)


class TestQrCreds(unittest.TestCase):

    def test_tlv_to_qr_basic(self):
        # Construct hex in S, P, I, E, C order
        # S: MyThreadNet -> 030b4d795468726561644e6574
        # P: 00112233445566778899AABBCCDDEEFF -> 051000112233445566778899AABBCCDDEEFF
        # I: 1234 -> 01021234
        # E: 1122334455667788 -> 02081122334455667788
        # C: 15 (Page 0) -> 000300000f

        tlv_hex = "030b4d795468726561644e6574" + \
                  "051000112233445566778899AABBCCDDEEFF" + \
                  "01021234" + \
                  "02081122334455667788" + \
                  "000300000f"

        expected_qr = "THREAD:S:MyThreadNet;P:00112233445566778899AABBCCDDEEFF;I:1234;E:1122334455667788;C:15;;"

        self.assertEqual(qrcreds.tlv_to_qr(tlv_hex), expected_qr)

    def test_qr_to_tlv_basic(self):
        qr_string = "THREAD:S:MyThreadNet;P:00112233445566778899AABBCCDDEEFF;I:1234;E:1122334455667788;C:15;;"

        # Expected hex should be uppercase
        expected_tlv_hex = "030B4D795468726561644E6574" + \
                           "051000112233445566778899AABBCCDDEEFF" + \
                           "01021234" + \
                           "02081122334455667788" + \
                           "00010F"

        self.assertEqual(qrcreds.qr_to_tlv(qr_string), expected_tlv_hex)

    def test_round_trip(self):
        # Input Hex (S, P, I, E, C order)
        tlv_hex = "030B4D795468726561644E6574" + \
                  "051000112233445566778899AABBCCDDEEFF" + \
                  "01021234" + \
                  "02081122334455667788" + \
                  "00010F"

        qr = qrcreds.tlv_to_qr(tlv_hex)
        res_hex = qrcreds.qr_to_tlv(qr)

        self.assertEqual(res_hex, tlv_hex)

    def test_channel_compact(self):
        # 00010f -> Channel 15 (Tag 0x00)
        tlv_hex = "00010f"
        expected_qr = "THREAD:C:15;;"

        self.assertEqual(qrcreds.tlv_to_qr(tlv_hex), expected_qr)

    def test_channel_consistency(self):
        # Verify that both standard (3-byte) and compact (1-byte) channel TLVs
        # result in the canonical compact TLV after round-trip.

        # Standard: 000300000F (Page 0, Channel 15)
        long_tlv = "000300000F"
        qr = qrcreds.tlv_to_qr(long_tlv)
        self.assertEqual(qr, "THREAD:C:15;;")

        # Round trip should produce the compact canonical form: 00010F
        final_tlv = qrcreds.qr_to_tlv(qr)
        self.assertEqual(final_tlv, "00010F")

    def test_pskc_round_trip(self):
        # PSKc (Tag 0x04)
        # Value: 00112233445566778899AABBCCDDEEFF
        pskc_hex = "00112233445566778899AABBCCDDEEFF"
        tlv_hex = "0410" + pskc_hex  # Tag 04, Length 16

        expected_qr = f"THREAD:K:{pskc_hex};;"

        # Test TLV -> QR
        self.assertEqual(qrcreds.tlv_to_qr(tlv_hex), expected_qr)

        # Test QR -> TLV
        self.assertEqual(qrcreds.qr_to_tlv(expected_qr), tlv_hex)

    def test_malformed_token_warning(self):
        # QR string with a malformed token 'Oops' (no colon)
        qr_string = "THREAD:S:MyThreadNet;Oops;P:00112233445566778899AABBCCDDEEFF;;"

        # Capture stderr to check for warning
        with contextlib.redirect_stderr(StringIO()) as stderr:
            # Should still process valid tokens despite the malformed one
            tlv_hex = qrcreds.qr_to_tlv(qr_string)

        # Check output is still valid for S and P fields
        # S: MyThreadNet -> 030B4D795468726561644E6574
        # P: Key -> 051000112233445566778899AABBCCDDEEFF
        expected_tlv = "030B4D795468726561644E6574" + \
                       "051000112233445566778899AABBCCDDEEFF"
        self.assertEqual(tlv_hex, expected_tlv)

        # Check for warning in stderr
        self.assertIn("Warning: Malformed token 'Oops' ignored.", stderr.getvalue())

    def test_invalid_qr_start(self):
        with self.assertRaises(ValueError):
            qrcreds.qr_to_tlv("INVALID:Start")

    def test_invalid_hex(self):
        with self.assertRaises(ValueError):
            qrcreds.tlv_to_qr("ZZZZ")


if __name__ == '__main__':
    unittest.main()
