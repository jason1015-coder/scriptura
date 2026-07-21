//! ## Task Runner
//!
//! Loads and executes tasks defined in JSON. Manages child processes
//! for build tasks, scripts, and other automated workflows.
//!
//! Replaces `TaskRunner` (QObject-based) from the original C++ codebase.

use std::collections::HashMap;
use std::ffi::c_void;
use std::io::BufRead;
use std::process::{Child, Command, Stdio};
use std::sync::mpsc::{self, Sender};
use std::sync::Mutex;
use std::thread;

use serde_json::Value;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);

#[derive(Debug, Clone)]
pub struct Task {
    pub name: String,
    pub cmd: String,
    pub args: Vec<String>,
    pub cwd: String,
    pub env: HashMap<String, String>,
}

pub struct TaskRunner {
    tasks: HashMap<String, Task>,
    current_process: Mutex<Option<Child>>,
    output_thread: Option<thread::JoinHandle<()>>,
    shutdown_tx: Option<Sender<()>>,

    on_started: Option<CbStr>,
    on_started_data: *mut c_void,
    on_finished: Option<CbStr>,
    on_finished_data: *mut c_void,
    on_output: Option<CbStr>,
    on_output_data: *mut c_void,
    on_error: Option<CbStr>,
    on_error_data: *mut c_void,
}

unsafe impl Send for TaskRunner {}
unsafe impl Sync for TaskRunner {}

impl TaskRunner {
    pub fn new() -> Self {
        Self {
            tasks: HashMap::new(),
            current_process: Mutex::new(None),
            output_thread: None,
            shutdown_tx: None,
            on_started: None,
            on_started_data: std::ptr::null_mut(),
            on_finished: None,
            on_finished_data: std::ptr::null_mut(),
            on_output: None,
            on_output_data: std::ptr::null_mut(),
            on_error: None,
            on_error_data: std::ptr::null_mut(),
        }
    }

    /// Load tasks from a JSON string.
    pub fn load(&mut self, json_tasks: &str) -> Result<(), String> {
        let parsed: Value = serde_json::from_str(json_tasks)
            .map_err(|e| format!("Invalid task JSON: {}", e))?;

        if let Some(tasks_array) = parsed.as_array() {
            for task_val in tasks_array {
                let name = task_val.get("name")
                    .and_then(|v| v.as_str())
                    .ok_or_else(|| "Task missing 'name'".to_string())?;

                let task = Task {
                    name: name.to_string(),
                    cmd: task_val.get("command").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                    args: task_val.get("args")
                        .and_then(|v| v.as_array())
                        .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                        .unwrap_or_default(),
                    cwd: task_val.get("cwd").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                    env: task_val.get("env")
                        .and_then(|v| v.as_object())
                        .map(|o| {
                            o.iter()
                                .map(|(k, v)| (k.clone(), v.as_str().unwrap_or("").to_string()))
                                .collect()
                        })
                        .unwrap_or_default(),
                };

                self.tasks.insert(name.to_string(), task);
            }
        }

        Ok(())
    }

    /// Run a task by name.
    pub fn run(&mut self, task_name: &str) {
        let task = match self.tasks.get(task_name) {
            Some(t) => t.clone(),
            None => {
                if let Some(cb) = self.on_error {
                    let err = format!("Task '{}' not found", task_name);
                    let msg = std::ffi::CString::new(&err[..]).unwrap_or_default();
                    cb(msg.as_ptr(), self.on_error_data);
                }
                return;
            }
        };

        let mut cmd = Command::new(&task.cmd);
        cmd.args(&task.args);
        if !task.cwd.is_empty() {
            cmd.current_dir(&task.cwd);
        }
        for (key, val) in &task.env {
            cmd.env(key, val);
        }

        cmd.stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        let mut child = match cmd.spawn() {
            Ok(c) => c,
            Err(e) => {
                if let Some(cb) = self.on_error {
                    let err = format!("Failed to start task: {}", e);
                    let msg = std::ffi::CString::new(&err[..]).unwrap_or_default();
                    cb(msg.as_ptr(), self.on_error_data);
                }
                return;
            }
        };

        let stdout = child.stdout.take();
        let _stderr = child.stderr.take();

        if let Ok(mut process) = self.current_process.lock() {
            *process = Some(child);
        }

        let (tx, rx) = mpsc::channel();
        self.shutdown_tx = Some(tx);

        // Fire started callback — scoped CString
        if let Some(cb) = self.on_started {
            let msg = std::ffi::CString::new(task_name).unwrap_or_default();
            cb(msg.as_ptr(), self.on_started_data);
        }

        let on_output = self.on_output;
        let on_output_data = self.on_output_data as usize;

        // Spawn output reader thread
        self.output_thread = Some(thread::spawn(move || {
            if let Some(stdout) = stdout {
                let reader = std::io::BufReader::new(stdout);
                for line in reader.lines() {
                    if rx.try_recv().is_ok() {
                        break;
                    }
                    if let Ok(line) = line {
                        if let Some(cb) = on_output {
                            let msg = std::ffi::CString::new(&line[..]).unwrap_or_default();
                            cb(msg.as_ptr(), on_output_data as *mut c_void);
                        }
                    }
                }
            }
        }));
    }

    /// Stop the currently running task.
    pub fn stop(&mut self) {
        if let Some(ref tx) = self.shutdown_tx {
            let _ = tx.send(());
        }
        if let Ok(mut process) = self.current_process.lock() {
            if let Some(ref mut child) = *process {
                let _ = child.kill();
                let _ = child.wait();
            }
            *process = None;
        }
    }

    /// Get list of available tasks.
    pub fn available_tasks(&self) -> Vec<String> {
        self.tasks.keys().cloned().collect()
    }

    // ── Callback setters ───────────────────────────────────────────

    pub fn set_on_started(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_started = Some(cb);
        self.on_started_data = data;
    }

    pub fn set_on_finished(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_finished = Some(cb);
        self.on_finished_data = data;
    }

    pub fn set_on_output(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_output = Some(cb);
        self.on_output_data = data;
    }

    pub fn set_on_error(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_error = Some(cb);
        self.on_error_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_runner() {
        let r = TaskRunner::new();
        assert!(r.available_tasks().is_empty());
    }

    #[test]
    fn test_load_simple_task() {
        let mut r = TaskRunner::new();
        let json = r#"[{"name": "build", "command": "make", "args": ["-j4"], "cwd": "."}]"#;
        r.load(json).unwrap();
        let tasks = r.available_tasks();
        assert_eq!(tasks, vec!["build"]);
    }

    #[test]
    fn test_load_multiple_tasks() {
        let mut r = TaskRunner::new();
        let json = r#"[
            {"name": "build", "command": "make"},
            {"name": "test", "command": "ctest"}
        ]"#;
        r.load(json).unwrap();
        let mut tasks = r.available_tasks();
        tasks.sort();
        assert_eq!(tasks, vec!["build", "test"]);
    }

    #[test]
    fn test_load_invalid_json() {
        let mut r = TaskRunner::new();
        let result = r.load("not valid json");
        assert!(result.is_err());
    }

    #[test]
    fn test_load_task_missing_name() {
        let mut r = TaskRunner::new();
        let result = r.load(r#"[{"command": "make"}]"#);
        assert!(result.is_err());
    }

    #[test]
    fn test_load_task_with_env() {
        let mut r = TaskRunner::new();
        let json = r#"[{"name": "build", "command": "make", "env": {"PATH": "/usr/bin"}}]"#;
        r.load(json).unwrap();
        assert_eq!(r.available_tasks(), vec!["build"]);
    }

    #[test]
    fn test_run_nonexistent_task_does_not_panic() {
        let mut r = TaskRunner::new();
        r.run("nonexistent");
    }

    #[test]
    fn test_stop_without_running_does_not_panic() {
        let mut r = TaskRunner::new();
        r.stop();
    }

    #[test]
    fn test_load_empty_json() {
        let mut r = TaskRunner::new();
        r.load("[]").unwrap();
        assert!(r.available_tasks().is_empty());
    }

    #[test]
    fn test_load_duplicate_task_overwrites() {
        let mut r = TaskRunner::new();
        r.load(r#"[{"name": "build", "command": "make"}]"#).unwrap();
        r.load(r#"[{"name": "build", "command": "cmake"}]"#).unwrap();
        assert_eq!(r.available_tasks().len(), 1);
    }
}
