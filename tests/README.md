# Thread Safety Tests

This directory contains thread safety tests for Solstice's synchronization primitives and concurrent components.

## Building

The tests are automatically included when building the Solstice project:

```bash
cmake --build build
```

## Running Tests

### Basic Test Run

```bash
./build/bin/ThreadSafetyTest
```

### With ThreadSanitizer (Recommended)

ThreadSanitizer (TSan) is a powerful tool for detecting data races and other thread safety issues.

#### Building with ThreadSanitizer

**Linux/macOS (GCC/Clang):**
```bash
cmake -B build -DENABLE_THREAD_SANITIZER=ON
cmake --build build
```

**Running:**
```bash
./build/bin/ThreadSafetyTest --tsan
```

**Note:** ThreadSanitizer requires a runtime library. On Linux, you may need:
```bash
sudo apt-get install libtsan0  # Ubuntu/Debian
```

#### Windows (MSVC)

MSVC doesn't have ThreadSanitizer, but you can use:
- **Visual Studio Static Analysis** (enabled automatically in the CMakeLists.txt)
- **AddressSanitizer** with `/fsanitize=address` (available in recent MSVC versions)

## Test Coverage

The test suite covers:

1. **Spinlock and LockGuard**
   - Concurrent access protection
   - Counter increment stress test

2. **ExecutionGuard**
   - Single execution enforcement
   - Concurrent execution attempts
   - Timeout mechanism

3. **BytecodeVM Thread Safety**
   - Concurrent `LoadProgram` calls
   - Concurrent `RegisterNative` calls
   - Concurrent `HasModule` checks

4. **JobSystem Thread Safety**
   - Concurrent job submission
   - Async job execution
   - Result retrieval

5. **Stress Test - Mixed Operations**
   - Combined stress test with all primitives
   - High concurrency scenarios

## Interpreting Results

### Normal Output
```
========================================
Solstice Thread Safety Test Suite
========================================

PASS: Spinlock and LockGuard thread safety
PASS: ExecutionGuard thread safety and timeout
PASS: BytecodeVM thread safety
PASS: JobSystem thread safety
PASS: Stress test with mixed operations

========================================
Test Results
========================================
Passed: 5
Failed: 0
Duration: 1234 ms

✓ All tests passed!
```

### ThreadSanitizer Output

If ThreadSanitizer detects issues, you'll see output like:
```
WARNING: ThreadSanitizer: data race
  Read of size 4 at 0x7f... by thread T2:
  Previous write of size 4 at 0x7f... by thread T1:
```

This indicates a data race that needs to be fixed.

## Continuous Integration

These tests should be run:
- Before every commit
- In CI/CD pipelines
- After major refactorings
- When adding new concurrent code

## Known Limitations

- `ProceduralTextureManager` is not fully tested here as it requires bgfx initialization
- Some tests may have slight timing variance (allowed in stress tests)
- ThreadSanitizer may report false positives in third-party libraries (can be suppressed)

## Troubleshooting

### Test Fails with "Counter mismatch"
- This indicates a race condition in the synchronization primitives
- Check that all shared state is properly protected
- Verify memory ordering is correct

### ThreadSanitizer Reports Issues
- Review the stack traces carefully
- Check if the issue is in Solstice code or third-party libraries
- Use suppressions file for known third-party issues

### Tests Hang
- Check for deadlocks in synchronization primitives
- Verify `ExecutionGuard` timeout mechanism is working
- Ensure `JobSystem` shutdown is properly implemented
