#!/bin/bash

# Run all tests and collect results
echo "==================================================================="
echo "                     MONITOR APPLICATION TEST SUITE"
echo "==================================================================="
echo "Date: $(date)"
echo "==================================================================="
echo

total_tests=0
passed_tests=0
failed_tests=0
crashed_tests=0

# Find all test executables
test_files=$(find . -maxdepth 1 -name "test_*" -type f -perm +111 2>/dev/null | sort)
total_files=$(echo "$test_files" | wc -l | tr -d ' ')

echo "Found $total_files test executables"
echo "==================================================================="
echo

for test in $test_files; do
    test_name=$(basename "$test")
    echo -n "Running $test_name... "
    
    # Run test with timeout and capture output
    if timeout 5 "$test" --help &>/dev/null; then
        # Test executable works, now run actual tests
        output=$(timeout 10 "$test" 2>&1)
        exit_code=$?
        
        if [ $exit_code -eq 0 ]; then
            # Try to extract test counts from output
            passed=$(echo "$output" | grep -E "(\[PASS\]|PASSED|passed|✓)" | wc -l | tr -d ' ')
            failed=$(echo "$output" | grep -E "(\[FAIL\]|FAILED|failed|✗)" | wc -l | tr -d ' ')
            
            if [ $passed -gt 0 ] || [ $failed -gt 0 ]; then
                echo "PASSED: $passed, FAILED: $failed"
                passed_tests=$((passed_tests + passed))
                failed_tests=$((failed_tests + failed))
                total_tests=$((total_tests + passed + failed))
            else
                # No clear pass/fail output, treat as single test
                echo "PASSED (1 test)"
                passed_tests=$((passed_tests + 1))
                total_tests=$((total_tests + 1))
            fi
        elif [ $exit_code -eq 124 ]; then
            echo "TIMEOUT"
            crashed_tests=$((crashed_tests + 1))
            total_tests=$((total_tests + 1))
        else
            echo "FAILED (exit code: $exit_code)"
            failed_tests=$((failed_tests + 1))
            total_tests=$((total_tests + 1))
        fi
    else
        echo "CRASHED (cannot run)"
        crashed_tests=$((crashed_tests + 1))
        total_tests=$((total_tests + 1))
    fi
done

echo
echo "==================================================================="
echo "                           TEST SUMMARY"
echo "==================================================================="
echo "Total Test Executables: $total_files"
echo "Total Tests Run: $total_tests"
echo "Tests Passed: $passed_tests"
echo "Tests Failed: $failed_tests"
echo "Tests Crashed/Timeout: $crashed_tests"
echo

if [ $failed_tests -eq 0 ] && [ $crashed_tests -eq 0 ]; then
    echo "SUCCESS RATE: 100%"
    echo "STATUS: ✅ ALL TESTS PASSED"
else
    success_rate=$(echo "scale=2; $passed_tests * 100 / $total_tests" | bc)
    echo "SUCCESS RATE: ${success_rate}%"
    echo "STATUS: ⚠️  SOME TESTS FAILED"
fi
echo "==================================================================="