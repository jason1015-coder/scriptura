#include "languageregistry.h"
#include <QFileInfo>

LanguageRegistry& LanguageRegistry::instance()
{
    static LanguageRegistry registry;
    return registry;
}

LanguageRegistry::LanguageRegistry()
{
    registerBuiltinLanguages();
}

void LanguageRegistry::registerLanguage(const LanguageDefinition &def)
{
    m_languages.append(def);
}

const LanguageDefinition* LanguageRegistry::findByName(const QString &name) const
{
    const QString lower = name.toLower();
    for (const LanguageDefinition &def : m_languages) {
        if (def.name == lower)
            return &def;
    }
    return nullptr;
}

const LanguageDefinition* LanguageRegistry::findByExtension(const QString &extension) const
{
    const QString lower = extension.toLower();
    for (const LanguageDefinition &def : m_languages) {
        if (def.extensions.contains(lower))
            return &def;
    }
    return nullptr;
}

QStringList LanguageRegistry::allLanguageNames() const
{
    QStringList names;
    for (const LanguageDefinition &def : m_languages)
        names.append(def.name);
    return names;
}

const QVector<LanguageDefinition>& LanguageRegistry::allLanguages() const
{
    return m_languages;
}

QString LanguageRegistry::languageForFile(const QString &filePath) const
{
    const QString extension = QFileInfo(filePath).suffix().toLower();
    const LanguageDefinition *def = findByExtension(extension);
    return def ? def->name : "text";
}

// ============================================================
// Builtin language registrations
// ============================================================

void LanguageRegistry::registerBuiltinLanguages()
{
    registerPython();
    registerCStyle();
    registerJava();
    registerJavaScript();
    registerTypeScript();
    registerRust();
    registerGo();
    registerShell();
    registerHtml();
    registerCss();
    registerScript();
    registerSwift();
    registerKotlin();
    registerRuby();
    registerPhp();
    registerCsharp();
    registerDart();
    registerLua();
    registerR();
    registerScala();
    registerObjectiveC();
    registerYaml();
    registerToml();
    registerJson();
    registerMarkdown();
    registerSql();
    registerPerl();
    registerHaskell();
    registerElixir();
}

void LanguageRegistry::registerPython()
{
    LanguageDefinition def;
    def.name = "python";
    def.extensions = {"py", "pyw", "pyx", "pxd", "pyi"};
    def.keywords = {"False", "None", "True", "and", "as", "assert", "async", "await",
                    "break", "class", "continue", "def", "del", "elif", "else", "except",
                    "finally", "for", "from", "global", "if", "import", "in", "is",
                    "lambda", "nonlocal", "not", "or", "pass", "raise", "return",
                    "try", "while", "with", "yield"};
    def.builtins = {"print", "len", "range", "str", "int", "float", "list", "dict",
                    "set", "tuple", "open", "sum", "enumerate", "zip", "map", "filter",
                    "sorted", "reversed", "abs", "round", "isinstance", "issubclass",
                    "super", "property", "staticmethod", "classmethod"};
    def.lineComment = "#";
    def.hasPythonTripleStrings = true;
    def.multiLineStringDelimiters = {"\"\"\"", "'''"};
    registerLanguage(def);
}

void LanguageRegistry::registerCStyle()
{
    // C and C++ share a very similar keyword set
    LanguageDefinition def;
    def.name = "cpp"; // Primary name
    def.extensions = {"c", "cpp", "cc", "cxx", "h", "hh", "hpp", "hxx", "c++", "h++"};
    def.keywords = {"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
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
                    "using", "virtual", "void", "volatile", "while", "xor", "xor_eq"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerJava()
{
    LanguageDefinition def;
    def.name = "java";
    def.extensions = {"java", "class", "jar", "jmod"};
    def.keywords = {"abstract", "assert", "boolean", "break", "byte", "case", "catch",
                    "char", "class", "const", "continue", "default", "do", "double",
                    "else", "enum", "extends", "final", "finally", "float", "for",
                    "goto", "if", "implements", "import", "instanceof", "int",
                    "interface", "long", "native", "new", "package", "private",
                    "protected", "public", "return", "short", "static", "strictfp",
                    "super", "switch", "synchronized", "this", "throw", "throws",
                    "transient", "try", "void", "volatile", "while", "var", "record",
                    "sealed", "permits", "yields"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerJavaScript()
{
    LanguageDefinition def;
    def.name = "javascript";
    def.extensions = {"js", "jsx", "mjs", "cjs", "es6"};
    def.keywords = {"async", "await", "break", "case", "catch", "class", "const",
                    "continue", "debugger", "default", "delete", "do", "else",
                    "export", "extends", "false", "finally", "for", "function",
                    "if", "import", "in", "instanceof", "let", "new", "null",
                    "of", "return", "super", "switch", "this", "throw", "true",
                    "try", "typeof", "undefined", "var", "void", "while", "with",
                    "yield", "static", "get", "set"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    def.templateStringDelimiter = "`";
    registerLanguage(def);
}

void LanguageRegistry::registerTypeScript()
{
    LanguageDefinition def;
    def.name = "typescript";
    def.extensions = {"ts", "tsx", "mts", "cts"};
    def.keywords = {"abstract", "any", "as", "async", "await", "boolean", "break",
                    "case", "catch", "class", "const", "continue", "debugger",
                    "declare", "default", "delete", "do", "else", "enum", "export",
                    "extends", "false", "finally", "for", "from", "function", "get",
                    "if", "implements", "import", "in", "instanceof", "interface",
                    "keyof", "let", "module", "namespace", "new", "never", "null",
                    "number", "object", "of", "private", "protected", "public",
                    "readonly", "return", "set", "static", "string", "super",
                    "switch", "symbol", "this", "throw", "true", "try", "type",
                    "typeof", "undefined", "unknown", "var", "void", "while",
                    "with", "yield"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    def.templateStringDelimiter = "`";
    registerLanguage(def);
}

void LanguageRegistry::registerRust()
{
    LanguageDefinition def;
    def.name = "rust";
    def.extensions = {"rs", "rlib"};
    def.keywords = {"as", "async", "await", "break", "const", "continue", "crate",
                    "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
                    "impl", "in", "let", "loop", "match", "mod", "move", "mut",
                    "pub", "ref", "return", "self", "Self", "static", "struct",
                    "super", "trait", "true", "type", "unsafe", "use", "where",
                    "while", "abstract", "become", "box", "do", "final", "macro",
                    "override", "priv", "typeof", "unsized", "virtual", "yield"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerGo()
{
    LanguageDefinition def;
    def.name = "go";
    def.extensions = {"go"};
    def.keywords = {"break", "default", "func", "interface", "select", "case",
                    "defer", "go", "map", "struct", "chan", "else", "goto",
                    "package", "switch", "const", "fallthrough", "if", "range",
                    "type", "continue", "for", "import", "return", "var"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerShell()
{
    LanguageDefinition def;
    def.name = "shell";
    def.extensions = {"sh", "bash", "zsh", "ksh", "fish", "shell"};
    def.keywords = {"if", "then", "else", "elif", "fi", "for", "while", "do",
                    "done", "case", "esac", "function", "select", "in", "time",
                    "until", "return", "exit", "export", "local", "declare",
                    "typeset", "readonly", "unset", "alias", "trap", "eval",
                    "exec", "let", "source", "."};
    def.lineComment = "#";
    // Shell has no block comments
    registerLanguage(def);
}

void LanguageRegistry::registerHtml()
{
    LanguageDefinition def;
    def.name = "html";
    def.extensions = {"html", "htm", "xhtml", "shtml"};
    def.hasHtmlComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerCss()
{
    LanguageDefinition def;
    def.name = "css";
    def.extensions = {"css", "scss", "sass", "less", "styl"};
    def.lineComment = "//";  // scss/sass/less use //
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerScript()
{
    LanguageDefinition def;
    def.name = "script";
    def.extensions = {"scr"};
    def.keywords = {"print", "let", "var", "true", "false"};
    def.lineComment = "#";
    registerLanguage(def);
}

// ============================================================
// New language registrations
// ============================================================

void LanguageRegistry::registerSwift()
{
    LanguageDefinition def;
    def.name = "swift";
    def.extensions = {"swift", "swiftmodule"};
    def.keywords = {"associatedtype", "async", "await", "as", "break", "case",
                    "catch", "class", "continue", "default", "defer", "deinit",
                    "do", "else", "enum", "extension", "fallthrough", "false",
                    "fileprivate", "for", "func", "guard", "if", "import", "in",
                    "init", "inout", "internal", "is", "let", "nonisolated",
                    "open", "operator", "private", "protocol", "public", "rethrows",
                    "return", "self", "Self", "static", "struct", "subscript",
                    "super", "throw", "throws", "true", "try", "typealias",
                    "var", "where", "while", "any", "some", "macro", "repeat",
                    "precedencegroup", "actor", "isolated", "nonisolated",
                    "consuming", "borrowing", "distributed"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    def.multiLineStringDelimiters = {"\"\"\""};
    registerLanguage(def);
}

void LanguageRegistry::registerKotlin()
{
    LanguageDefinition def;
    def.name = "kotlin";
    def.extensions = {"kt", "kts", "ktm"};
    def.keywords = {"abstract", "actual", "annotation", "as", "as?", "break",
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
                    "when", "where", "while"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerRuby()
{
    LanguageDefinition def;
    def.name = "ruby";
    def.extensions = {"rb", "ruby", "erb", "gemspec", "rake", "Gemfile"};
    def.keywords = {"BEGIN", "END", "alias", "and", "begin", "break", "case",
                    "class", "def", "defined?", "do", "else", "elsif", "end",
                    "ensure", "false", "for", "if", "in", "module", "next",
                    "nil", "not", "or", "redo", "rescue", "retry", "return",
                    "self", "super", "then", "true", "undef", "unless", "until",
                    "when", "while", "yield", "__ENCODING__", "__LINE__", "__FILE__"};
    def.lineComment = "#";
    registerLanguage(def);
}

void LanguageRegistry::registerPhp()
{
    LanguageDefinition def;
    def.name = "php";
    def.extensions = {"php", "phtml", "php3", "php4", "php5", "php7", "phps"};
    def.keywords = {"abstract", "and", "array", "as", "break", "callable",
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
                    "unset", "use", "var", "while", "xor", "yield"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerCsharp()
{
    LanguageDefinition def;
    def.name = "csharp";
    def.extensions = {"cs", "csx"};
    def.keywords = {"abstract", "as", "async", "await", "base", "bool", "break",
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
                    "volatile", "while"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerDart()
{
    LanguageDefinition def;
    def.name = "dart";
    def.extensions = {"dart"};
    def.keywords = {"abstract", "as", "assert", "async", "await", "break",
                    "case", "catch", "class", "const", "continue", "covariant",
                    "default", "deferred", "do", "dynamic", "else", "enum",
                    "export", "extends", "extension", "external", "factory",
                    "false", "final", "finally", "for", "Function", "get",
                    "hide", "if", "implements", "import", "in", "interface",
                    "is", "late", "library", "mixin", "native", "new", "null",
                    "of", "on", "operator", "optional", "part", "required",
                    "rethrow", "return", "set", "show", "static", "super",
                    "switch", "sync", "this", "throw", "true", "try", "typedef",
                    "var", "void", "while", "with", "yield"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerLua()
{
    LanguageDefinition def;
    def.name = "lua";
    def.extensions = {"lua", "wlua"};
    def.keywords = {"and", "break", "do", "else", "elseif", "end", "false",
                    "for", "function", "goto", "if", "in", "local", "nil",
                    "not", "or", "repeat", "return", "then", "true", "until",
                    "while"};
    def.lineComment = "--";
    def.blockCommentStart = "--[[";
    def.blockCommentEnd = "]]";
    registerLanguage(def);
}

void LanguageRegistry::registerR()
{
    LanguageDefinition def;
    def.name = "r";
    def.extensions = {"r", "R", "rmd", "rda", "rds"};
    def.keywords = {"if", "else", "repeat", "while", "function", "for", "in",
                    "next", "break", "TRUE", "FALSE", "NULL", "Inf", "NaN",
                    "NA", "NA_integer_", "NA_real_", "NA_complex_",
                    "NA_character_", "return", "library", "require", "source",
                    "setwd", "getwd", "install", "packages", "data", "rm",
                    "ls", "list", "matrix", "data.frame", "c", "factor",
                    "as.numeric", "as.character", "as.factor", "as.integer",
                    "as.logical", "summary", "plot", "print", "cat",
                    "nrow", "ncol", "length", "names", "rownames", "colnames",
                    "head", "tail", "subset", "transform", "aggregate",
                    "apply", "lapply", "sapply", "tapply", "mapply"};
    def.lineComment = "#";
    registerLanguage(def);
}

void LanguageRegistry::registerScala()
{
    LanguageDefinition def;
    def.name = "scala";
    def.extensions = {"scala", "sc", "sbt"};
    def.keywords = {"abstract", "case", "catch", "class", "def", "do", "else",
                    "enum", "export", "extends", "false", "final", "finally",
                    "for", "forSome", "given", "if", "implicit", "import",
                    "lazy", "macro", "match", "new", "null", "object", "override",
                    "package", "private", "protected", "public", "return",
                    "sealed", "super", "then", "throw", "trait", "true", "try",
                    "type", "using", "val", "var", "while", "with", "yield"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerObjectiveC()
{
    LanguageDefinition def;
    def.name = "objectivec";
    def.extensions = {"m", "mm"};  // .h is owned by C/C++
    def.keywords = {"auto", "break", "case", "char", "const", "continue",
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
                    "unsafe_unretained"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerYaml()
{
    LanguageDefinition def;
    def.name = "yaml";
    def.extensions = {"yaml", "yml"};
    def.lineComment = "#";
    registerLanguage(def);
}

void LanguageRegistry::registerToml()
{
    LanguageDefinition def;
    def.name = "toml";
    def.extensions = {"toml"};
    def.lineComment = "#";
    registerLanguage(def);
}

void LanguageRegistry::registerJson()
{
    LanguageDefinition def;
    def.name = "json";
    def.extensions = {"json", "jsonc"};
    def.lineComment = "//";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerMarkdown()
{
    LanguageDefinition def;
    def.name = "markdown";
    def.extensions = {"md", "markdown", "mdown", "mdwn", "mkd", "mkdown"};
    def.lineComment = "";  // No line comments in Markdown
    registerLanguage(def);
}

void LanguageRegistry::registerSql()
{
    LanguageDefinition def;
    def.name = "sql";
    def.extensions = {"sql", "mysql", "pgsql", "sqlite"};
    def.keywords = {"SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES",
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
                    "WITH", "RECURSIVE", "RETURNING", "EXPLAIN", "ANALYZE"};
    def.lineComment = "--";
    def.blockCommentStart = "/*";
    def.blockCommentEnd = "*/";
    def.hasCStyleComments = true;
    registerLanguage(def);
}

void LanguageRegistry::registerPerl()
{
    LanguageDefinition def;
    def.name = "perl";
    def.extensions = {"pl", "pm", "t", "pod", "PL"};
    def.keywords = {"if", "elsif", "else", "unless", "while", "until", "for",
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
                    "gmtime", "localtime", "time", "sleep"};
    def.lineComment = "#";
    registerLanguage(def);
}

void LanguageRegistry::registerHaskell()
{
    LanguageDefinition def;
    def.name = "haskell";
    def.extensions = {"hs", "lhs", "hsc"};
    def.keywords = {"as", "case", "class", "data", "default", "deriving",
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
                    "Floating", "Real", "RealFloat", "RealFrac"};
    def.lineComment = "--";
    def.blockCommentStart = "{-";
    def.blockCommentEnd = "-}";
    registerLanguage(def);
}

void LanguageRegistry::registerElixir()
{
    LanguageDefinition def;
    def.name = "elixir";
    def.extensions = {"ex", "exs"};
    def.keywords = {"true", "false", "nil", "and", "or", "not", "when",
                    "in", "not in", "fn", "do", "end", "catch", "rescue",
                    "after", "else", "raise", "throw", "unless", "case",
                    "cond", "if", "for", "with", "receive", "after",
                    "def", "defp", "defmodule", "defprotocol", "defimpl",
                    "defstruct", "defexception", "defmacro", "defmacrop",
                    "defguard", "defguardp", "delegate", "import",
                    "require", "use", "alias", "super", "quote", "unquote",
                    "var!", "sigil", "reraise", "try", "exit"};
    def.lineComment = "#";
    registerLanguage(def);
}
