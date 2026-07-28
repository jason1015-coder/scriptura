use std::path::Path;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestResult {
    pub name: String,
    pub status: String, // "passed", "failed", "skipped", "error"
    pub message: String,
    pub duration_ms: i32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestSuite {
    pub name: String,
    pub framework: String,
    pub results: Vec<TestResult>,
    pub passed: i32,
    pub failed: i32,
    pub skipped: i32,
    pub total: i32,
}

pub fn detect_framework(project_path: &str) -> String {
    let path = Path::new(project_path);

    // Cargo.toml → cargo test
    if path.join("Cargo.toml").exists() {
        return "cargo_test".to_string();
    }

    // package.json with test script → jest/npm
    if let Some(scripts) = read_package_json_scripts(path) {
        if scripts.contains("test") {
            return "jest".to_string();
        }
    }

    // go.mod → go test
    if path.join("go.mod").exists() {
        return "go_test".to_string();
    }

    // Python test files → pytest
    if has_python_tests(path) {
        return "pytest".to_string();
    }

    // CMakeLists.txt with enable_testing → ctest
    if path.join("CMakeLists.txt").exists() {
        return "cmake_ctest".to_string();
    }

    "unknown".to_string()
}

pub fn build_command(framework: &str, project_path: &str, filter: &str) -> String {
    match framework {
        "cargo_test" => {
            if filter.is_empty() {
                "cargo test".to_string()
            } else {
                format!("cargo test {}", filter)
            }
        }
        "jest" => {
            if filter.is_empty() {
                "npx jest --verbose".to_string()
            } else {
                format!("npx jest --testPathPattern={}", filter)
            }
        }
        "go_test" => {
            if filter.is_empty() {
                "go test -v ./...".to_string()
            } else {
                format!("go test -v -run {} ./...", filter)
            }
        }
        "pytest" => {
            if filter.is_empty() {
                "python -m pytest -v --tb=short".to_string()
            } else {
                format!("python -m pytest -v --tb=short -k {}", filter)
            }
        }
        "cmake_ctest" => "ctest --output-on-failure".to_string(),
        _ => String::new(),
    }
}

pub fn parse_output(framework: &str, output: &str) -> TestSuite {
    match framework {
        "cargo_test" => parse_cargo_test(output),
        "jest" => parse_jest(output),
        "go_test" => parse_go_test(output),
        "pytest" => parse_pytest(output),
        _ => TestSuite {
            name: framework.to_string(),
            framework: framework.to_string(),
            results: Vec::new(),
            passed: 0,
            failed: 0,
            skipped: 0,
            total: 0,
        },
    }
}

fn parse_cargo_test(output: &str) -> TestSuite {
    let mut results = Vec::new();
    let mut passed = 0;
    let mut failed = 0;
    let mut skipped = 0;

    for line in output.lines() {
        let trimmed = line.trim();
        // test test_name ... ok
        // test test_name ... FAILED
        // test test_name ... ignored
        if trimmed.starts_with("test ") {
            let parts: Vec<&str> = trimmed.splitn(4, ' ').collect();
            if parts.len() >= 4 {
                let name = parts[1].to_string();
                let status_str = parts[3].trim_end_matches('\n');
                let (status, count_type) = match status_str {
                    "ok" => ("passed", "passed"),
                    "FAILED" => ("failed", "failed"),
                    s if s.starts_with("ignored") => ("skipped", "skipped"),
                    _ => ("error", "failed"),
                };
                results.push(TestResult {
                    name,
                    status: status.to_string(),
                    message: String::new(),
                    duration_ms: 0,
                });
                match count_type {
                    "passed" => passed += 1,
                    "failed" => failed += 1,
                    "skipped" => skipped += 1,
                    _ => {}
                }
            }
        }
    }

    TestSuite {
        name: "cargo test".to_string(),
        framework: "cargo_test".to_string(),
        total: passed + failed + skipped,
        passed,
        failed,
        skipped,
        results,
    }
}

fn parse_pytest(output: &str) -> TestSuite {
    let mut results = Vec::new();
    let mut passed = 0;
    let mut failed = 0;
    let mut skipped = 0;

    for line in output.lines() {
        let trimmed = line.trim();
        // PASSED test_file.py::test_name
        // FAILED test_file.py::test_name - message
        if trimmed.starts_with("PASSED")
            || trimmed.starts_with("FAILED")
            || trimmed.starts_with("ERROR")
            || trimmed.starts_with("SKIPPED")
        {
            let parts: Vec<&str> = trimmed.splitn(2, ' ').collect();
            if parts.len() >= 2 {
                let status_str = parts[0];
                let name = parts[1].to_string();
                let status = match status_str {
                    "PASSED" => {
                        passed += 1;
                        "passed"
                    }
                    "FAILED" | "ERROR" => {
                        failed += 1;
                        "failed"
                    }
                    "SKIPPED" => {
                        skipped += 1;
                        "skipped"
                    }
                    _ => "error",
                };
                results.push(TestResult {
                    name,
                    status: status.to_string(),
                    message: String::new(),
                    duration_ms: 0,
                });
            }
        }
    }

    TestSuite {
        name: "pytest".to_string(),
        framework: "pytest".to_string(),
        total: passed + failed + skipped,
        passed,
        failed,
        skipped,
        results,
    }
}

fn parse_jest(output: &str) -> TestSuite {
    let mut results = Vec::new();
    let mut passed = 0;
    let mut failed = 0;
    let mut skipped = 0;

    for line in output.lines() {
        let trimmed = line.trim();
        // ✓ test name (123 ms)
        // ✕ test name (123 ms)
        if trimmed.starts_with('✓') || trimmed.starts_with('✕') {
            let name = trimmed[2..].to_string();
            let (status, count_type) = if trimmed.starts_with('✓') {
                ("passed", "passed")
            } else {
                ("failed", "failed")
            };
            results.push(TestResult {
                name,
                status: status.to_string(),
                message: String::new(),
                duration_ms: 0,
            });
            match count_type {
                "passed" => passed += 1,
                "failed" => failed += 1,
                _ => {}
            }
        } else if trimmed.starts_with('-') || trimmed.starts_with('○') {
            // skipped tests
            skipped += 1;
            let name = trimmed[2..].trim().to_string();
            results.push(TestResult {
                name,
                status: "skipped".to_string(),
                message: String::new(),
                duration_ms: 0,
            });
        }
    }

    TestSuite {
        name: "jest".to_string(),
        framework: "jest".to_string(),
        total: passed + failed + skipped,
        passed,
        failed,
        skipped,
        results,
    }
}

fn parse_go_test(output: &str) -> TestSuite {
    let mut results = Vec::new();
    let mut passed = 0;
    let mut failed = 0;
    let mut skipped = 0;

    for line in output.lines() {
        let trimmed = line.trim();
        // --- PASS: TestName (0.00s)
        // --- FAIL: TestName (0.00s)
        // --- SKIP: TestName (0.00s)
        if trimmed.starts_with("--- ") {
            let parts: Vec<&str> = trimmed.splitn(3, ' ').collect();
            if parts.len() >= 3 {
                let status_str = parts[1].trim_end_matches(':');
                let name = parts[2].to_string();
                let (status, count_type) = match status_str {
                    "PASS" => ("passed", "passed"),
                    "FAIL" => ("failed", "failed"),
                    "SKIP" => ("skipped", "skipped"),
                    _ => ("error", "failed"),
                };
                results.push(TestResult {
                    name,
                    status: status.to_string(),
                    message: String::new(),
                    duration_ms: 0,
                });
                match count_type {
                    "passed" => passed += 1,
                    "failed" => failed += 1,
                    "skipped" => skipped += 1,
                    _ => {}
                }
            }
        }
    }

    TestSuite {
        name: "go test".to_string(),
        framework: "go_test".to_string(),
        total: passed + failed + skipped,
        passed,
        failed,
        skipped,
        results,
    }
}

// ── Helper functions ─────────────────────────────────────────────

fn read_package_json_scripts(path: &Path) -> Option<String> {
    let file_path = path.join("package.json");
    let content = std::fs::read_to_string(file_path).ok()?;
    let v: serde_json::Value = serde_json::from_str(&content).ok()?;
    Some(v.get("scripts")?.to_string())
}

fn has_python_tests(path: &Path) -> bool {
    // Check for common Python test indicators
    if path.join("tests").exists() || path.join("test").exists() {
        return true;
    }
    for ext in &["py"] {
        if let Ok(entries) = std::fs::read_dir(path) {
            for entry in entries.flatten() {
                let name = entry.file_name();
                let name_str = name.to_string_lossy();
                if name_str.starts_with("test_") && name_str.ends_with(&format!(".{}", ext)) {
                    return true;
                }
            }
        }
    }
    false
}
