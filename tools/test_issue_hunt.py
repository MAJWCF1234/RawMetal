import unittest
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

from hunt_issues import STATIC, campaign_names, extract_findings, iteration_seed, newer_sources, parse_maps, registered_tests, select_checks, verify_audit, verify_detail, verify_report


class IssueHuntTest(unittest.TestCase):
    def test_discovery_rejects_duplicate_names(self):
        source = 'const std::pair<const char*,bool(*)()> tests[]={{"one",fn},{"one",fn}};'
        with self.assertRaises(ValueError):
            registered_tests(source)

    def test_never_accepts_a_skipped_or_incomplete_test(self):
        for report in ('ALL PASSED 0\n', 'PASS other 1s\nALL PASSED 0\n',
                       'PASS movement 1s\n', 'FAIL movement 1s\nALL PASSED 0\n',
                       'PASS movement 1s\nPASS other 1s\nALL PASSED 0\n'):
            self.assertFalse(verify_report('movement', 0, False, report)[0])

    def test_rejects_crashes_and_timeouts_even_with_a_pass_report(self):
        report = 'PASS movement 1s\nALL PASSED 0\n'
        self.assertTrue(verify_report('movement', 0, False, report)[0])
        self.assertFalse(verify_report('movement', 1, False, report)[0])
        self.assertFalse(verify_report('movement', None, True, report)[0])

    def test_detail_reports_must_contain_a_pass_and_no_failure(self):
        for report in ('', 'some output\n', 'PASS first\nFAIL second\n',
                       'first: PASS\nsecond: FAIL\n', 'checks=4 result=FAIL\n'):
            self.assertFalse(verify_detail(0, False, report)[0], report)
        for report in ('PASS: 0 placement problems\n', 'first: PASS\n', 'checks=4 result=PASS\n'):
            self.assertTrue(verify_detail(0, False, report)[0], report)
        self.assertFalse(verify_detail(21, False, 'PASS earlier\n')[0])
        self.assertFalse(verify_detail(None, True, 'PASS earlier\n')[0])

    def test_map_selection_is_bounded_unique_and_inclusive(self):
        self.assertEqual(parse_maps('0-2,64,179'), [0, 1, 2, 64, 179])
        for value in ('', '4-2', '0-180', '2,2', '-1', '1,,2'):
            with self.assertRaises(ValueError, msg=value):
                parse_maps(value)

    def test_map_audit_requires_the_right_map_and_a_complete_reach_grid(self):
        report = 'Map 64 (10,2 z -19): 50 reachable cells (40 standing) of 9216\n' + ('.' + '#' * 23 + '\n') + ('#' * 24 + '\n') * 23 + 'PASS: 0 placement problems\n'
        self.assertTrue(verify_audit(64, 0, False, report)[0])
        self.assertFalse(verify_audit(63, 0, False, report)[0])
        self.assertFalse(verify_audit(64, 0, False, report.replace('#' * 24 + '\n', '', 1))[0])
        self.assertTrue(verify_audit(64, 0, False, report.replace('40 standing', '51 standing'))[0])
        self.assertFalse(verify_audit(64, 0, False, report.replace('40 standing', '0 standing'))[0])
        self.assertFalse(verify_audit(64, 0, False, report.replace('.' + '#' * 23, '#' * 24))[0])
        self.assertFalse(verify_audit(64, 0, False, report + 'FAIL: 1 placement problems\n')[0])

    def test_map_findings_preserve_actionable_diagnostics(self):
        report = 'INFO ROUTE: 0\nTRAP 10 cells at 8,13\nUNREACHABLE creature[0]\nFAIL: 2 placement problems\n'
        self.assertEqual(extract_findings('audit', report), ['TRAP 10 cells at 8,13', 'UNREACHABLE creature[0]'])

    def test_profiles_compose_without_skipping_or_duplicating_requests(self):
        selected = select_checks(['movement'], 'deep-ai-query', True, False, True, [64])
        self.assertEqual(selected.count('deep-ai-query'), 1)
        self.assertIn('deep-guard-combat', selected)
        self.assertIn('map-64', selected)
        self.assertNotIn('movement', selected)
        self.assertEqual(select_checks(['movement'], None, False, False, False, []), ['movement'])

    def test_static_profile_is_external_and_has_a_report_contract(self):
        self.assertEqual(STATIC['static-integrity'][0], 'static_issue_checks.py')
        self.assertEqual(STATIC['static-integrity'][1], 'static-check.txt')

    def test_repeated_runs_advance_and_wrap_the_reproducible_seed(self):
        self.assertEqual([iteration_seed(0xfffffffe, i) for i in (1, 2, 3)], [0xfffffffe, 0xffffffff, 0])

    def test_map_names_ignore_comments_and_missing_catalogs(self):
        self.assertEqual(campaign_names('CampaignMapNames{{ "FIRST", // "ignored"\n "SECOND" }};'), ['FIRST', 'SECOND'])
        self.assertEqual(campaign_names('no catalog'), [])

    def test_stale_build_warning_ignores_third_party_and_old_sources(self):
        workspace = Path(__file__).resolve().parents[1] / '.build'
        workspace.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=workspace) as folder:
            root = Path(folder)
            (root / 'src' / 'ThirdParty').mkdir(parents=True)
            exe, current, older, vendor = root / 'game.exe', root / 'src' / 'new.cpp', root / 'src' / 'old.h', root / 'src' / 'ThirdParty' / 'vendor.cpp'
            for path in (exe, current, older, vendor):
                path.write_text('fixture')
            for path, timestamp in ((exe, 100), (current, 200), (older, 50), (vendor, 200)):
                os.utime(path, (timestamp, timestamp))
            with patch('hunt_issues.ROOT', root):
                self.assertEqual(newer_sources(exe), [str(Path('src') / 'new.cpp')])


if __name__ == '__main__':
    unittest.main()
