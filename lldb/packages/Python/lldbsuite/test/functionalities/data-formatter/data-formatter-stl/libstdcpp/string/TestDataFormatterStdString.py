# coding=utf8
"""
Test lldb data formatter subsystem.
"""

from __future__ import print_function


import os
import time
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class StdStringDataFormatterTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line number to break at.
        self.line = line_number('main.cpp', '// Set break point at this line.')

    @skipIfWindows  # libstdcpp not ported to Windows
    @skipIfwatchOS  # libstdcpp not ported to watchos
    def test_with_run_command(self):
        """Test that that file and class static variables display correctly."""
        self.build()
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)

        lldbutil.run_break_set_by_file_and_line(
            self, "main.cpp", self.line, num_expected_locations=-1)

        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
                    substrs=['stopped',
                             'stop reason = breakpoint'])

        # This is the function to remove the custom formats in order to have a
        # clean slate for the next test case.
        def cleanup():
            self.runCmd('type format clear', check=False)
            self.runCmd('type summary clear', check=False)
            self.runCmd('type filter clear', check=False)
            self.runCmd('type synth clear', check=False)
            self.runCmd(
                "settings set target.max-children-count 256",
                check=False)

        # Execute the cleanup function during test case tear down.
        self.addTearDownHook(cleanup)

        var_s = self.frame().FindVariable('s')
        var_S = self.frame().FindVariable('S')
        var_mazeltov = self.frame().FindVariable('mazeltov')
        var_q = self.frame().FindVariable('q')
        var_Q = self.frame().FindVariable('Q')

        self.assertTrue(
            var_s.GetSummary() == 'L"hello world! ?????? ??????!"',
            "s summary wrong")
        self.assertTrue(var_S.GetSummary() == 'L"!!!!"', "S summary wrong")
        self.assertTrue(
            var_mazeltov.GetSummary() == 'L"?????? ??????"',
            "mazeltov summary wrong")
        self.assertTrue(
            var_q.GetSummary() == '"hello world"',
            "q summary wrong")
        self.assertTrue(
            var_Q.GetSummary() == '"quite a long std::strin with lots of info inside it"',
            "Q summary wrong")

        self.runCmd("next")

        self.assertTrue(
            var_S.GetSummary() == 'L"!!!!!"',
            "new S summary wrong")
