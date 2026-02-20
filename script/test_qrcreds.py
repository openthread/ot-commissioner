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
        # S: MyThreadNet -> 040b4d795468726561644e6574
        # P: 00112233445566778899AABBCCDDEEFF -> 051000112233445566778899AABBCCDDEEFF
        # I: 1234 -> 02021234
        # E: 1122334455667788 -> 03081122334455667788
        # C: 15 (Page 0) -> 010300000f
        
        tlv_hex = "040b4d795468726561644e6574" + \
                  "051000112233445566778899AABBCCDDEEFF" + \
                  "02021234" + \
                  "03081122334455667788" + \
                  "010300000f"
                  
        expected_qr = "THREAD:S:MyThreadNet;P:00112233445566778899AABBCCDDEEFF;I:1234;E:1122334455667788;C:15;;"
        
        self.assertEqual(qrcreds.tlv_to_qr(tlv_hex), expected_qr)

    def test_qr_to_tlv_basic(self):
        qr_string = "THREAD:S:MyThreadNet;P:00112233445566778899AABBCCDDEEFF;I:1234;E:1122334455667788;C:15;;"
        
        # Expected hex should be uppercase
        expected_tlv_hex = "040B4D795468726561644E6574" + \
                           "051000112233445566778899AABBCCDDEEFF" + \
                           "02021234" + \
                           "03081122334455667788" + \
                           "010300000F"
                  
        self.assertEqual(qrcreds.qr_to_tlv(qr_string), expected_tlv_hex)

    def test_round_trip(self):
        # Input Hex (S, P, I, E, C order)
        tlv_hex = "040B4D795468726561644E6574" + \
                  "051000112233445566778899AABBCCDDEEFF" + \
                  "02021234" + \
                  "03081122334455667788" + \
                  "010300000F"
                  
        qr = qrcreds.tlv_to_qr(tlv_hex)
        res_hex = qrcreds.qr_to_tlv(qr)
        
        self.assertEqual(res_hex, tlv_hex)

    def test_channel_compact_tlv(self):
        # Test C: 15 with compact TLV format (1 byte length) if supported by script?
        # Script supports reading compact 1 byte:
        # if length == 1: fields['C'] = str(value[0])
        
        # 01010f -> Channel 15
        tlv_hex = "01010f"
        # Everything else missing
        expected_qr = "THREAD:C:15;;"
        
        self.assertEqual(qrcreds.tlv_to_qr(tlv_hex), expected_qr)
        
    def test_invalid_qr_start(self):
        # Suppress stderr to keep test output clean
        with contextlib.redirect_stderr(StringIO()):
            with self.assertRaises(SystemExit):
                qrcreds.qr_to_tlv("INVALID:Start")

    def test_invalid_hex(self):
        # Suppress stderr to keep test output clean
        with contextlib.redirect_stderr(StringIO()):
            with self.assertRaises(SystemExit) as cm:
                qrcreds.tlv_to_qr("ZZZZ") 
            self.assertEqual(cm.exception.code, 1)

if __name__ == '__main__':
    unittest.main()
