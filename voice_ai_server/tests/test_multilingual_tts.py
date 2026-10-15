import unittest

from app.config import Settings
from app.services import TextToSpeech, normalize_stt_language, normalize_tts_language


class MultilingualTtsTests(unittest.TestCase):
    def setUp(self):
        self.tts = TextToSpeech(Settings())

    def test_language_aliases(self):
        self.assertEqual(normalize_tts_language("en-US"), "en")
        self.assertEqual(normalize_tts_language("ja"), "ja")
        self.assertEqual(normalize_tts_language("jpn"), "ja")

    def test_blank_stt_language_enables_auto_detection(self):
        self.assertIsNone(normalize_stt_language(None))
        self.assertIsNone(normalize_stt_language(""))
        self.assertIsNone(normalize_stt_language("   "))
        self.assertEqual(normalize_stt_language("JA"), "ja")

    def test_default_voices_match_language(self):
        self.assertEqual(self.tts.default_voice("en"), "af_heart")
        self.assertEqual(self.tts.default_voice("ja"), "jf_alpha")

    def test_unsupported_language_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "not supported"):
            normalize_tts_language("fr")


if __name__ == "__main__":
    unittest.main()
