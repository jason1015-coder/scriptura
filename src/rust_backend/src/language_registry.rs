//! ## Language Registry
//!
//! Owns all programming-language definitions for the editor:
//! - Language id/name, file extensions, keywords, builtins
//! - Comment styles (line/block/C-style/HTML/Python-triple)
//! - String delimiter configuration (incl. JS/TS template literals)
//!
//! Ported verbatim from the original C++ `LanguageRegistry` builtins.
//! The C++ side keeps only a parsed view (for the Qt highlighter); this
//! module is the single source of truth.

use std::collections::HashMap;
use serde::{Deserialize, Serialize};
use serde_json::json;

/// Full language definition (camelCase JSON — consumed by the C++ facade).
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LanguageDefinition {
    pub id: String,
    pub name: String,
    /// File extensions WITHOUT leading dot (e.g. "py", "pyw").
    #[serde(default)]
    pub extensions: Vec<String>,
    #[serde(default)]
    pub keywords: Vec<String>,
    #[serde(default)]
    pub builtins: Vec<String>,
    #[serde(default)]
    pub block_comment_start: String,
    #[serde(default)]
    pub block_comment_end: String,
    #[serde(default)]
    pub line_comment: String,
    #[serde(default)]
    pub has_c_style_comments: bool,
    #[serde(default)]
    pub has_html_comments: bool,
    #[serde(default)]
    pub has_python_triple_strings: bool,
    #[serde(default = "default_true")]
    pub has_bracket_matching: bool,
    #[serde(default = "default_string_delimiters")]
    pub string_delimiters: Vec<String>,
    #[serde(default)]
    pub multi_line_string_delimiters: Vec<String>,
    #[serde(default)]
    pub template_string_delimiter: String,
    // Legacy LSP server hints (kept for the legacy register() API).
    #[serde(default)]
    pub server_command: Option<String>,
    #[serde(default)]
    pub server_args: Vec<String>,
}

fn default_true() -> bool { true }
fn default_string_delimiters() -> Vec<String> { vec!["\"".to_string(), "'".to_string()] }

impl LanguageDefinition {
    fn minimal(id: &str, name: &str, extensions: &str) -> Self {
        Self {
            id: id.to_string(),
            name: name.to_string(),
            extensions: split_extensions(extensions),
            keywords: Vec::new(),
            builtins: Vec::new(),
            block_comment_start: String::new(),
            block_comment_end: String::new(),
            line_comment: String::new(),
            has_c_style_comments: false,
            has_html_comments: false,
            has_python_triple_strings: false,
            has_bracket_matching: true,
            string_delimiters: default_string_delimiters(),
            multi_line_string_delimiters: Vec::new(),
            template_string_delimiter: String::new(),
            server_command: None,
            server_args: Vec::new(),
        }
    }
}

/// Split a comma-separated extension list, lowercasing and stripping dots.
fn split_extensions(extensions: &str) -> Vec<String> {
    extensions.split(',')
        .map(|s| s.trim().trim_start_matches('.').to_string())
        .filter(|s| !s.is_empty())
        .collect()
}

pub struct LanguageRegistry {
    /// lang id -> definition
    languages: HashMap<String, LanguageDefinition>,
    /// ".ext" (or full lowercase filename like "gemfile") -> lang id
    extension_map: HashMap<String, String>,
    /// Insertion order of language ids (stable JSON + first-match-wins).
    order: Vec<String>,
}

impl LanguageRegistry {
    pub fn new() -> Self {
        let mut reg = Self {
            languages: HashMap::new(),
            extension_map: HashMap::new(),
            order: Vec::new(),
        };
        reg.register_builtin();
        reg
    }

    fn insert(&mut self, def: LanguageDefinition) {
        let id = def.id.clone();
        for ext in &def.extensions {
            self.extension_map.insert(ext_key(ext), id.clone());
        }
        if !self.languages.contains_key(&id) {
            self.order.push(id.clone());
        }
        self.languages.insert(id, def);
    }

    /// Register a language from the legacy (id, name, extensions, server) API.
    pub fn register(&mut self, lang_id: &str, name: &str, extensions: &str,
                    server_command: Option<&str>, server_args: &[&str]) {
        let mut def = LanguageDefinition::minimal(lang_id, name, extensions);
        def.server_command = server_command.map(|s| s.to_string());
        def.server_args = server_args.iter().map(|s| s.to_string()).collect();
        self.insert(def);
    }

    /// Register (or overwrite) a language from a full definition.
    pub fn register_definition(&mut self, def: LanguageDefinition) {
        self.insert(def);
    }

    /// Register from a full definition JSON document.
    pub fn register_definition_json(&mut self, json: &str) -> Result<(), String> {
        let def: LanguageDefinition = serde_json::from_str(json)
            .map_err(|e| e.to_string())?;
        if def.id.is_empty() {
            return Err("definition has empty id".to_string());
        }
        self.register_definition(def);
        Ok(())
    }

    /// Unregister a language.
    pub fn unregister(&mut self, lang_id: &str) {
        if let Some(entry) = self.languages.remove(lang_id) {
            for ext in &entry.extensions {
                self.extension_map.remove(&ext_key(ext));
            }
            self.order.retain(|id| id != lang_id);
        }
    }

    /// Legacy JSON view: extensions rendered WITH dot (".rs").
    /// Contains the full definition plus legacy server fields.
    pub fn get(&self, lang_id: &str) -> Option<String> {
        self.languages.get(lang_id).map(|d| {
            let exts: Vec<String> = d.extensions.iter().map(|e| format!(".{}", e)).collect();
            json!({
                "id": d.id,
                "name": d.name,
                "extensions": exts,
                "keywords": d.keywords,
                "builtins": d.builtins,
                "blockCommentStart": d.block_comment_start,
                "blockCommentEnd": d.block_comment_end,
                "lineComment": d.line_comment,
                "hasCStyleComments": d.has_c_style_comments,
                "hasHtmlComments": d.has_html_comments,
                "hasPythonTripleStrings": d.has_python_triple_strings,
                "hasBracketMatching": d.has_bracket_matching,
                "stringDelimiters": d.string_delimiters,
                "multiLineStringDelimiters": d.multi_line_string_delimiters,
                "templateStringDelimiter": d.template_string_delimiter,
                "serverCommand": d.server_command,
                "serverArgs": d.server_args,
            }).to_string()
        })
    }

    /// Full definition as JSON (canonical: extensions without dot).
    pub fn definition_json(&self, lang_id: &str) -> Option<String> {
        self.languages.get(lang_id)
            .map(|d| serde_json::to_string(d).unwrap_or_default())
    }

    /// JSON array of ALL definitions in registration order.
    pub fn definitions_json(&self) -> String {
        let defs: Vec<&LanguageDefinition> = self.order.iter()
            .filter_map(|id| self.languages.get(id))
            .collect();
        serde_json::to_string(&defs).unwrap_or_default()
    }

    /// Detect language id from filename (mirrors the C++ suffix-matching
    /// semantics: full filename first for dot-less entries like "Gemfile",
    /// then the text after the last dot).
    pub fn detect(&self, filename: &str) -> Option<&str> {
        let fname = filename.to_lowercase();

        // Try full filename first (for Makefile, Gemfile, CMakeLists.txt, ...).
        if let Some(lang_id) = self.extension_map.get(&fname) {
            return Some(lang_id.as_str());
        }

        // Try extension (text after the last dot).
        if let Some(dot) = fname.rfind('.') {
            if let Some(lang_id) = self.extension_map.get(&fname[dot + 1..]) {
                return Some(lang_id.as_str());
            }
        }

        None
    }

    /// Language id for a file path; falls back to "text" (matches the
    /// original C++ LanguageRegistry::languageForFile contract).
    pub fn language_for_file(&self, path: &str) -> String {
        let fname = path.rsplit(['/', '\\']).next().unwrap_or(path);
        let ext = fname.rsplit('.').next().unwrap_or("");
        if !ext.is_empty() && ext != fname.to_lowercase() {
            for id in &self.order {
                if let Some(d) = self.languages.get(id) {
                    if d.extensions.iter().any(|e| e.eq_ignore_ascii_case(ext)) {
                        return d.id.clone();
                    }
                }
            }
        }
        "text".to_string()
    }

    /// List all registered language ids (insertion order).
    pub fn languages(&self) -> Vec<String> {
        self.order.clone()
    }

    fn register_builtin(&mut self) {
        let builtins: Vec<LanguageDefinition> = vec![
            builtin("python", &["py", "pyw", "pyx", "pxd", "pyi"], &[
                "False", "None", "True", "and", "as", "assert", "async", "await",
                "break", "class", "continue", "def", "del", "elif", "else", "except",
                "finally", "for", "from", "global", "if", "import", "in", "is",
                "lambda", "nonlocal", "not", "or", "pass", "raise", "return",
                "try", "while", "with", "yield"],
                "#", "", "", false)
                .with_builtins(&["print", "len", "range", "str", "int", "float", "list", "dict",
                    "set", "tuple", "open", "sum", "enumerate", "zip", "map", "filter",
                    "sorted", "reversed", "abs", "round", "isinstance", "issubclass",
                    "super", "property", "staticmethod", "classmethod"])
                .with_python_triple(),
            builtin("cpp", &["c", "cpp", "cc", "cxx", "h", "hh", "hpp", "hxx", "c++", "h++"], &[
                "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
                "bitor", "break", "case", "catch", "char", "char8_t", "char16_t",
                "char32_t", "class", "compl", "concept", "const", "consteval",
                "constexpr", "constinit", "const_cast", "continue", "co_await",
                "co_return", "co_yield", "decltype", "default", "delete", "do",
                "double", "dynamic_cast", "else", "enum", "explicit", "export",
                "extern", "false", "float", "for", "friend", "goto", "if",
                "inline", "int", "long", "mutable", "namespace", "new",
                "noexcept", "not", "not_eq", "nullptr", "operator", "or",
                "or_eq", "private", "protected", "public", "register",
                "reinterpret_cast", "requires", "return", "short", "signed",
                "sizeof", "static", "static_assert", "static_cast", "struct",
                "switch", "template", "this", "thread_local", "throw", "true",
                "try", "typedef", "typeid", "typename", "union", "unsigned",
                "using", "virtual", "void", "volatile", "while", "xor", "xor_eq"],
                "//", "/*", "*/", true),
            builtin("java", &["java", "class", "jar", "jmod"], &[
                "abstract", "assert", "boolean", "break", "byte", "case", "catch",
                "char", "class", "const", "continue", "default", "do", "double",
                "else", "enum", "extends", "final", "finally", "float", "for",
                "goto", "if", "implements", "import", "instanceof", "int",
                "interface", "long", "native", "new", "package", "private",
                "protected", "public", "return", "short", "static", "strictfp",
                "super", "switch", "synchronized", "this", "throw", "throws",
                "transient", "try", "void", "volatile", "while", "var", "record",
                "sealed", "permits", "yields"],
                "//", "/*", "*/", true),
            builtin("javascript", &["js", "jsx", "mjs", "cjs", "es6"], &[
                "async", "await", "break", "case", "catch", "class", "const",
                "continue", "debugger", "default", "delete", "do", "else",
                "export", "extends", "false", "finally", "for", "function",
                "if", "import", "in", "instanceof", "let", "new", "null",
                "of", "return", "super", "switch", "this", "throw", "true",
                "try", "typeof", "undefined", "var", "void", "while", "with",
                "yield", "static", "get", "set"],
                "//", "/*", "*/", true)
                .with_template_string(),
            builtin("typescript", &["ts", "tsx", "mts", "cts"], &[
                "abstract", "any", "as", "async", "await", "boolean", "break",
                "case", "catch", "class", "const", "continue", "debugger",
                "declare", "default", "delete", "do", "else", "enum", "export",
                "extends", "false", "finally", "for", "from", "function", "get",
                "if", "implements", "import", "in", "instanceof", "interface",
                "keyof", "let", "module", "namespace", "new", "never", "null",
                "number", "object", "of", "private", "protected", "public",
                "readonly", "return", "set", "static", "string", "super",
                "switch", "symbol", "this", "throw", "true", "try", "type",
                "typeof", "undefined", "unknown", "var", "void", "while",
                "with", "yield"],
                "//", "/*", "*/", true)
                .with_template_string(),
            builtin("rust", &["rs", "rlib"], &[
                "as", "async", "await", "break", "const", "continue", "crate",
                "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
                "impl", "in", "let", "loop", "match", "mod", "move", "mut",
                "pub", "ref", "return", "self", "Self", "static", "struct",
                "super", "trait", "true", "type", "unsafe", "use", "where",
                "while", "abstract", "become", "box", "do", "final", "macro",
                "override", "priv", "typeof", "unsized", "virtual", "yield"],
                "//", "/*", "*/", true),
            builtin("go", &["go"], &[
                "break", "default", "func", "interface", "select", "case",
                "defer", "go", "map", "struct", "chan", "else", "goto",
                "package", "switch", "const", "fallthrough", "if", "range",
                "type", "continue", "for", "import", "return", "var"],
                "//", "/*", "*/", true),
            builtin("shell", &["sh", "bash", "zsh", "ksh", "fish", "shell"], &[
                "if", "then", "else", "elif", "fi", "for", "while", "do",
                "done", "case", "esac", "function", "select", "in", "time",
                "until", "return", "exit", "export", "local", "declare",
                "typeset", "readonly", "unset", "alias", "trap", "eval",
                "exec", "let", "source", "."],
                "#", "", "", false),
            builtin("html", &["html", "htm", "xhtml", "shtml"], &[],
                "", "", "", false)
                .with_html_comments(),
            builtin("css", &["css", "scss", "sass", "less", "styl"], &[],
                "//", "/*", "*/", true),
            builtin("script", &["scr"], &["print", "let", "var", "true", "false"],
                "#", "", "", false),
            builtin("swift", &["swift", "swiftmodule"], &[
                "associatedtype", "async", "await", "as", "break", "case",
                "catch", "class", "continue", "default", "defer", "deinit",
                "do", "else", "enum", "extension", "fallthrough", "false",
                "fileprivate", "for", "func", "guard", "if", "import", "in",
                "init", "inout", "internal", "is", "let", "nonisolated",
                "open", "operator", "private", "protocol", "public", "rethrows",
                "return", "self", "Self", "static", "struct", "subscript",
                "super", "throw", "throws", "true", "try", "typealias",
                "var", "where", "while", "any", "some", "macro", "repeat",
                "precedencegroup", "actor", "isolated", "nonisolated",
                "consuming", "borrowing", "distributed"],
                "//", "/*", "*/", true)
                .with_multi_line(&["\"\"\""]),
            builtin("kotlin", &["kt", "kts", "ktm"], &[
                "abstract", "actual", "annotation", "as", "as?", "break",
                "by", "catch", "class", "companion", "const", "constructor",
                "continue", "crossinline", "data", "delegate", "do", "dynamic",
                "else", "enum", "expect", "external", "false", "field",
                "file", "final", "finally", "for", "fun", "if", "import",
                "in", "!in", "infix", "init", "inline", "inner", "interface",
                "internal", "is", "!is", "it", "lateinit", "noinline",
                "null", "object", "open", "operator", "out", "override",
                "param", "private", "property", "protected", "public",
                "receiver", "reified", "return", "sealed", "setparam",
                "super", "suspend", "tailrec", "this", "throw", "true",
                "try", "typealias", "typeof", "val", "var", "vararg",
                "when", "where", "while"],
                "//", "/*", "*/", true),
            builtin("ruby", &["rb", "ruby", "erb", "gemspec", "rake", "Gemfile"], &[
                "BEGIN", "END", "alias", "and", "begin", "break", "case",
                "class", "def", "defined?", "do", "else", "elsif", "end",
                "ensure", "false", "for", "if", "in", "module", "next",
                "nil", "not", "or", "redo", "rescue", "retry", "return",
                "self", "super", "then", "true", "undef", "unless", "until",
                "when", "while", "yield", "__ENCODING__", "__LINE__", "__FILE__"],
                "#", "", "", false),
            builtin("php", &["php", "phtml", "php3", "php4", "php5", "php7", "phps"], &[
                "abstract", "and", "array", "as", "break", "callable",
                "case", "catch", "class", "clone", "const", "continue",
                "declare", "default", "die", "do", "echo", "else", "elseif",
                "empty", "enddeclare", "endfor", "endforeach", "endif",
                "endswitch", "endwhile", "eval", "exit", "extends", "final",
                "finally", "fn", "for", "foreach", "function", "global",
                "goto", "if", "implements", "include", "include_once",
                "instanceof", "insteadof", "interface", "isset", "list",
                "match", "namespace", "new", "or", "print", "private",
                "protected", "public", "readonly", "require", "require_once",
                "return", "static", "switch", "throw", "trait", "try",
                "unset", "use", "var", "while", "xor", "yield"],
                "//", "/*", "*/", true),
            builtin("csharp", &["cs", "csx"], &[
                "abstract", "as", "async", "await", "base", "bool", "break",
                "byte", "case", "catch", "char", "checked", "class", "const",
                "continue", "decimal", "default", "delegate", "do", "double",
                "else", "enum", "event", "explicit", "extern", "false",
                "finally", "fixed", "float", "for", "foreach", "goto", "if",
                "implicit", "in", "int", "interface", "internal", "is",
                "lock", "long", "namespace", "new", "null", "object",
                "operator", "out", "override", "params", "private",
                "protected", "public", "readonly", "record", "ref",
                "return", "sbyte", "sealed", "short", "sizeof", "stackalloc",
                "static", "string", "struct", "switch", "this", "throw",
                "true", "try", "typeof", "uint", "ulong", "unchecked",
                "unsafe", "ushort", "using", "var", "virtual", "void",
                "volatile", "while"],
                "//", "/*", "*/", true),
            builtin("dart", &["dart"], &[
                "abstract", "as", "assert", "async", "await", "break",
                "case", "catch", "class", "const", "continue", "covariant",
                "default", "deferred", "do", "dynamic", "else", "enum",
                "export", "extends", "extension", "external", "factory",
                "false", "final", "finally", "for", "Function", "get",
                "hide", "if", "implements", "import", "in", "interface",
                "is", "late", "library", "mixin", "native", "new", "null",
                "of", "on", "operator", "optional", "part", "required",
                "rethrow", "return", "set", "show", "static", "super",
                "switch", "sync", "this", "throw", "true", "try", "typedef",
                "var", "void", "while", "with", "yield"],
                "//", "/*", "*/", true),
            builtin("lua", &["lua", "wlua"], &[
                "and", "break", "do", "else", "elseif", "end", "false",
                "for", "function", "goto", "if", "in", "local", "nil",
                "not", "or", "repeat", "return", "then", "true", "until",
                "while"],
                "--", "--[[", "]]", false),
            builtin("r", &["r", "R", "rmd", "rda", "rds"], &[
                "if", "else", "repeat", "while", "function", "for", "in",
                "next", "break", "TRUE", "FALSE", "NULL", "Inf", "NaN",
                "NA", "NA_integer_", "NA_real_", "NA_complex_",
                "NA_character_", "return", "library", "require", "source",
                "setwd", "getwd", "install", "packages", "data", "rm",
                "ls", "list", "matrix", "data.frame", "c", "factor",
                "as.numeric", "as.character", "as.factor", "as.integer",
                "as.logical", "summary", "plot", "print", "cat",
                "nrow", "ncol", "length", "names", "rownames", "colnames",
                "head", "tail", "subset", "transform", "aggregate",
                "apply", "lapply", "sapply", "tapply", "mapply"],
                "#", "", "", false),
            builtin("scala", &["scala", "sc", "sbt"], &[
                "abstract", "case", "catch", "class", "def", "do", "else",
                "enum", "export", "extends", "false", "final", "finally",
                "for", "forSome", "given", "if", "implicit", "import",
                "lazy", "macro", "match", "new", "null", "object", "override",
                "package", "private", "protected", "public", "return",
                "sealed", "super", "then", "throw", "trait", "true", "try",
                "type", "using", "val", "var", "while", "with", "yield"],
                "//", "/*", "*/", true),
            builtin("objectivec", &["m", "mm"], &[
                "auto", "break", "case", "char", "const", "continue",
                "default", "do", "double", "else", "enum", "extern",
                "float", "for", "goto", "if", "int", "long", "register",
                "return", "short", "signed", "sizeof", "static", "struct",
                "switch", "typedef", "union", "unsigned", "void",
                "volatile", "while", "@interface", "@implementation",
                "@protocol", "@end", "@private", "@protected", "@public",
                "@property", "@synthesize", "@dynamic", "@selector",
                "@class", "@encode", "@synchronized", "@try", "@throw",
                "@catch", "@finally", "@autoreleasepool", "@package",
                "BOOL", "YES", "NO", "nil", "Nil", "NULL", "id",
                "Class", "SEL", "IMP", "self", "super", "_cmd",
                "instancetype", "nullable", "nonnull", "null_unspecified",
                "__kindof", "oneway", "in", "out", "inout", "bycopy",
                "byref", "assign", "retain", "copy", "readonly",
                "readwrite", "nonatomic", "atomic", "strong", "weak",
                "unsafe_unretained"],
                "//", "/*", "*/", true),
            builtin("yaml", &["yaml", "yml"], &[], "#", "", "", false),
            builtin("toml", &["toml"], &[], "#", "", "", false),
            builtin("json", &["json", "jsonc"], &[], "//", "/*", "*/", true),
            builtin("markdown", &["md", "markdown", "mdown", "mdwn", "mkd", "mkdown"], &[],
                "", "", "", false),
            builtin("sql", &["sql", "mysql", "pgsql", "sqlite"], &[
                "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES",
                "UPDATE", "SET", "DELETE", "CREATE", "TABLE", "ALTER",
                "DROP", "INDEX", "VIEW", "TRIGGER", "PROCEDURE", "FUNCTION",
                "IF", "ELSE", "THEN", "END", "BEGIN", "COMMIT", "ROLLBACK",
                "GRANT", "REVOKE", "JOIN", "LEFT", "RIGHT", "INNER",
                "OUTER", "FULL", "ON", "AND", "OR", "NOT", "IN", "IS",
                "NULL", "LIKE", "BETWEEN", "EXISTS", "ALL", "ANY",
                "ORDER", "BY", "GROUP", "HAVING", "LIMIT", "OFFSET",
                "DISTINCT", "AS", "CASE", "WHEN", "UNION", "EXCEPT",
                "INTERSECT", "PRIMARY", "KEY", "FOREIGN", "REFERENCES",
                "CASCADE", "CHECK", "DEFAULT", "CONSTRAINT", "UNIQUE",
                "INDEX", "ASC", "DESC", "COUNT", "SUM", "AVG", "MIN",
                "MAX", "CAST", "COALESCE", "NULLIF", "TRUE", "FALSE",
                "WITH", "RECURSIVE", "RETURNING", "EXPLAIN", "ANALYZE"],
                "--", "/*", "*/", true),
            builtin("perl", &["pl", "pm", "t", "pod", "PL"], &[
                "if", "elsif", "else", "unless", "while", "until", "for",
                "foreach", "do", "sub", "my", "our", "local", "state",
                "use", "require", "no", "package", "BEGIN", "END",
                "CHECK", "INIT", "UNITCHECK", "return", "last", "next",
                "redo", "goto", "die", "warn", "exit", "eval", "defined",
                "undef", "ref", "bless", "tie", "untie", "tied",
                "wantarray", "scalar", "push", "pop", "shift", "unshift",
                "splice", "sort", "map", "grep", "keys", "values",
                "each", "delete", "exists", "print", "say", "open",
                "close", "read", "write", "sysopen", "sysread",
                "syswrite", "seek", "tell", "truncate", "flock",
                "chmod", "chown", "mkdir", "rmdir", "link", "unlink",
                "symlink", "readlink", "rename", "glob", "chdir",
                "exec", "system", "fork", "wait", "waitpid",
                "gmtime", "localtime", "time", "sleep"],
                "#", "", "", false),
            builtin("haskell", &["hs", "lhs", "hsc"], &[
                "as", "case", "class", "data", "default", "deriving",
                "do", "else", "family", "forall", "foreign", "hiding",
                "if", "import", "in", "infix", "infixl", "infixr",
                "instance", "let", "module", "newtype", "of", "open",
                "pattern", "qualified", "role", "safe", "standalone",
                "then", "type", "unsafe", "where", "pure", "return",
                "fmap", ">>=", ">>", "fail", "True", "False",
                "Maybe", "Just", "Nothing", "Either", "Left", "Right",
                "IO", "IOError", "Integer", "Int", "Float", "Double",
                "Bool", "Char", "String", "Ord", "Eq", "Show", "Read",
                "Enum", "Bounded", "Num", "Integral", "Fractional",
                "Floating", "Real", "RealFloat", "RealFrac"],
                "--", "{-", "-}", false),
            builtin("elixir", &["ex", "exs"], &[
                "true", "false", "nil", "and", "or", "not", "when",
                "in", "not in", "fn", "do", "end", "catch", "rescue",
                "after", "else", "raise", "throw", "unless", "case",
                "cond", "if", "for", "with", "receive", "after",
                "def", "defp", "defmodule", "defprotocol", "defimpl",
                "defstruct", "defexception", "defmacro", "defmacrop",
                "defguard", "defguardp", "delegate", "import",
                "require", "use", "alias", "super", "quote", "unquote",
                "var!", "sigil", "reraise", "try", "exit"],
                "#", "", "", false),
        ];
        for def in builtins {
            self.insert(def);
        }
    }
}

impl Default for LanguageRegistry {
    fn default() -> Self { Self::new() }
}

/// Index key for an extension entry: lowercase, leading dot stripped.
/// Dot-less entries (e.g. "Gemfile") match the full lowercase filename.
fn ext_key(ext: &str) -> String {
    ext.trim().trim_start_matches('.').to_lowercase()
}

/// Builtin helper: constructs a definition with canonical defaults.
fn builtin(id: &str, extensions: &[&str], keywords: &[&str],
           line_comment: &str, block_start: &str, block_end: &str,
           c_style: bool) -> LanguageDefinition {
    let mut def = LanguageDefinition::minimal(id, id, &extensions.join(","));
    def.keywords = keywords.iter().map(|s| s.to_string()).collect();
    def.line_comment = line_comment.to_string();
    def.block_comment_start = block_start.to_string();
    def.block_comment_end = block_end.to_string();
    def.has_c_style_comments = c_style;
    def
}

impl LanguageDefinition {
    fn with_builtins(mut self, builtins: &[&str]) -> Self {
        self.builtins = builtins.iter().map(|s| s.to_string()).collect();
        self
    }
    fn with_python_triple(mut self) -> Self {
        self.has_python_triple_strings = true;
        self.multi_line_string_delimiters = vec!["\"\"\"".to_string(), "'''".to_string()];
        self
    }
    fn with_html_comments(mut self) -> Self {
        self.has_html_comments = true;
        self
    }
    fn with_template_string(mut self) -> Self {
        self.template_string_delimiter = "`".to_string();
        self
    }
    fn with_multi_line(mut self, delims: &[&str]) -> Self {
        self.multi_line_string_delimiters = delims.iter().map(|s| s.to_string()).collect();
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_registry_has_builtins() {
        let reg = LanguageRegistry::new();
        assert!(reg.languages().len() >= 20);
        assert!(reg.get("rust").is_some());
        assert!(reg.get("python").is_some());
        assert!(reg.get("javascript").is_some());
    }

    #[test]
    fn test_detect_by_extension() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("main.rs"), Some("rust"));
        assert_eq!(reg.detect("main.py"), Some("python"));
        assert_eq!(reg.detect("index.js"), Some("javascript"));
        assert_eq!(reg.detect("index.tsx"), Some("typescript"));
    }

    #[test]
    fn test_detect_by_full_filename() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("Gemfile"), Some("ruby"));
        // Legacy register API accepts dotted extensions.
        let mut reg2 = LanguageRegistry::new();
        reg2.register("cmake", "CMake", "CMakeLists.txt,.cmake", None, &[]);
        assert_eq!(reg2.detect("CMakeLists.txt"), Some("cmake"));
        assert_eq!(reg2.detect("foo.cmake"), Some("cmake"));
    }

    #[test]
    fn test_detect_unknown_extension() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("file.xyz"), None);
    }

    #[test]
    fn test_detect_no_extension() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("Makefile"), None);
    }

    #[test]
    fn test_detect_case_insensitive() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("Main.RS"), Some("rust"));
        assert_eq!(reg.detect("Main.PY"), Some("python"));
    }

    #[test]
    fn test_get_rust_info() {
        let reg = LanguageRegistry::new();
        let info = reg.get("rust").unwrap();
        assert!(info.contains("\"rust\""));
        assert!(info.contains(".rs"));
    }

    #[test]
    fn test_get_nonexistent() {
        let reg = LanguageRegistry::new();
        assert!(reg.get("nonexistent").is_none());
    }

    #[test]
    fn test_register_new_language() {
        let mut reg = LanguageRegistry::new();
        reg.register("my_lang", "My Language", ".my,.myl",
                     Some("my-lsp"), &["--flag", "value"]);
        let info = reg.get("my_lang").unwrap();
        assert!(info.contains("My Language"));
        assert!(info.contains("my-lsp"));
        assert!(info.contains("--flag"));
    }

    #[test]
    fn test_unregister_language() {
        let mut reg = LanguageRegistry::new();
        reg.register("custom", "Custom", ".cust", None, &[]);
        assert!(reg.get("custom").is_some());
        reg.unregister("custom");
        assert!(reg.get("custom").is_none());
        // Extension mapping should also be removed
        assert_eq!(reg.detect("file.cust"), None);
    }

    #[test]
    fn test_unregister_builtin() {
        let mut reg = LanguageRegistry::new();
        reg.unregister("rust");
        assert!(reg.get("rust").is_none());
        assert_eq!(reg.detect("main.rs"), None);
    }

    #[test]
    fn test_unregister_nonexistent() {
        let mut reg = LanguageRegistry::new();
        reg.unregister("nonexistent"); // Should not panic
    }

    #[test]
    fn test_register_overwrites() {
        let mut reg = LanguageRegistry::new();
        reg.register("rust", "My Rust", ".my_rs", None, &[]);
        let info = reg.get("rust").unwrap();
        assert!(info.contains("My Rust"));
        assert!(!info.contains(".rs"));
    }

    #[test]
    fn test_languages_list() {
        let mut reg = LanguageRegistry::new();
        let before = reg.languages().len();
        reg.register("extra", "Extra", ".extra", None, &[]);
        assert_eq!(reg.languages().len(), before + 1);
    }

    #[test]
    fn test_detect_dotfiles() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect(".bashrc"), None);
    }

    #[test]
    fn test_get_json_format() {
        let reg = LanguageRegistry::new();
        let info = reg.get("json").unwrap();
        assert!(info.contains("\"id\":\"json\"") || info.contains("\"id\": \"json\""));
        assert!(info.contains("\"name\":\"json\"") || info.contains("\"name\": \"json\""));
    }

    #[test]
    fn test_definitions_json_is_camel_case_array() {
        let reg = LanguageRegistry::new();
        let json = reg.definitions_json();
        assert!(json.starts_with('['));
        assert!(json.contains("\"keywords\""));
        assert!(json.contains("\"hasCStyleComments\""));
        assert!(json.contains("\"blockCommentStart\""));
        assert!(!json.contains(".rs\"")); // canonical extensions have no dot
    }

    #[test]
    fn test_definition_json_single() {
        let reg = LanguageRegistry::new();
        let json = reg.definition_json("python").unwrap();
        assert!(json.contains("\"def\""));
        assert!(json.contains("\"hasPythonTripleStrings\":true"));
        assert!(reg.definition_json("nope").is_none());
    }

    #[test]
    fn test_register_definition_json_roundtrip() {
        let mut reg = LanguageRegistry::new();
        reg.register_definition_json(
            r#"{"id":"zig","name":"zig","extensions":["zig"],
                "keywords":["fn","pub","const"],"lineComment":"//",
                "hasCStyleComments":true}"#).unwrap();
        assert_eq!(reg.detect("main.zig"), Some("zig"));
        let json = reg.definition_json("zig").unwrap();
        assert!(json.contains("\"fn\""));
        // C++ defaults restored on parse
        assert!(json.contains("\"hasBracketMatching\":true"));
        assert!(json.contains("\"stringDelimiters\":[\"\\\"\",\"'\"]"));
        assert!(reg.register_definition_json("not json").is_err());
        assert!(reg.register_definition_json(r#"{"name":"no-id"}"#).is_err());
    }

    #[test]
    fn test_language_for_file() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.language_for_file("/a/b/main.rs"), "rust");
        assert_eq!(reg.language_for_file("/a/b/main.PY"), "python");
        assert_eq!(reg.language_for_file("/a/b/notes.xyz"), "text");
        assert_eq!(reg.language_for_file("/a/b/Makefile"), "text");
        assert_eq!(reg.language_for_file("code.cpp"), "cpp");
    }
}
