use std::env;
use std::ffi::{c_void, CString};
use std::os::raw::{c_char, c_int};
use std::ptr;

extern "C" {
    fn cpp_qt_create_app(argc: c_int, argv: *mut *mut c_char) -> *mut c_void;
    fn cpp_qt_destroy_app(app: *mut c_void);
    fn cpp_qt_set_org_name(app: *mut c_void, name: *const c_char);
    fn cpp_qt_set_app_name(app: *mut c_void, name: *const c_char);
    fn cpp_qt_set_style(app: *mut c_void, style: *const c_char);
    fn cpp_qt_set_window_icon(app: *mut c_void);
    fn cpp_qt_load_translations(app: *mut c_void);
    fn cpp_qt_load_theme(app: *mut c_void, theme_id: i32, mode: i32);
    fn cpp_qt_create_main_window(
        app: *mut c_void,
        rust_app: *mut c_void,
        project: *const c_char,
        files: *const *const c_char,
        file_count: c_int,
    ) -> *mut c_void;
    fn cpp_qt_show_window(app: *mut c_void, window: *mut c_void);
    fn cpp_qt_run_event_loop(app: *mut c_void) -> c_int;
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let c_args: Vec<*mut c_char> = args.iter()
        .map(|s| CString::new(s.as_str()).unwrap().into_raw())
        .collect();

    let app = unsafe { scriptura_backend::rust_app_new() };
    unsafe { scriptura_backend::rust_app_initialize(app) };

    let qt_app = unsafe {
        cpp_qt_create_app(args.len() as c_int, c_args.as_ptr() as *mut *mut c_char)
    };

    let mut project_ptr: *const c_char = ptr::null();
    let mut file_ptrs: Vec<*const c_char> = Vec::new();
    let mut i = 1;
    while i < args.len() {
        if args[i] == "--project" && i + 1 < args.len() {
            let proj = CString::new(args[i + 1].clone()).unwrap();
            project_ptr = proj.into_raw();
            i += 2;
        } else {
            let path = CString::new(args[i].clone()).unwrap();
            file_ptrs.push(path.into_raw());
            i += 1;
        }
    }

    let exit_code = unsafe {
        cpp_qt_set_org_name(qt_app, b"Scriptura\0".as_ptr() as *const c_char);
        cpp_qt_set_app_name(qt_app, b"Scriptura\0".as_ptr() as *const c_char);
        cpp_qt_set_style(qt_app, b"Fusion\0".as_ptr() as *const c_char);
        cpp_qt_set_window_icon(qt_app);
        cpp_qt_load_translations(qt_app);
        cpp_qt_load_theme(qt_app, 0, 0);

        let main_window = cpp_qt_create_main_window(
            qt_app, app as *mut c_void,
            project_ptr,
            if file_ptrs.is_empty() { ptr::null() } else { file_ptrs.as_ptr() },
            file_ptrs.len() as c_int,
        );
        cpp_qt_show_window(qt_app, main_window);

        let code = cpp_qt_run_event_loop(qt_app);
        cpp_qt_destroy_app(qt_app);
        code
    };

    unsafe {
        scriptura_backend::rust_app_shutdown(app);
        scriptura_backend::rust_app_free(app);
    }

    std::process::exit(exit_code);
}
