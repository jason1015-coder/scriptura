/// Emmet abbreviation parser and expander for HTML/CSS.
///
/// This is a port of the C++ `EmmetParser` (now deleted) so the whole
/// parser lives in Rust and C++ only calls `rust_emmet_expand`.
/// Supports: tag expansion, nesting (`>`), siblings (`+`), climb-up (`^`),
/// multiplication (`*N`), classes/IDs, attributes, text content, and
/// CSS shorthand expansion.
use std::collections::HashMap;

/// Self-closing HTML tags
const SELF_CLOSING_TAGS: &[&str] = &[
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr",
];

/// CSS shorthand property mappings
fn css_shorthand_map() -> HashMap<&'static str, &'static str> {
    let mut m = HashMap::new();
    m.insert("m", "margin");
    m.insert("p", "padding");
    m.insert("fs", "font-size");
    m.insert("fw", "font-weight");
    m.insert("lh", "line-height");
    m.insert("ws", "white-space");
    m.insert("ov", "overflow");
    m.insert("pos", "position");
    m.insert("d", "display");
    m.insert("fd", "flex-direction");
    m.insert("fg", "flex-grow");
    m.insert("ji", "justify-items");
    m.insert("ai", "align-items");
    m.insert("bc", "border-color");
    m.insert("bgc", "background-color");
    m.insert("c", "color");
    m.insert("ff", "font-family");
    m.insert("bp", "breakpoint");
    m.insert("br", "border-radius");
    m.insert("sh", "box-shadow");
    m.insert("tac", "text-align");
    m.insert("tal", "text-align");
    m.insert("tar", "text-align");
    m.insert("o", "opacity");
    m.insert("z", "z-index");
    m.insert("g", "gap");
    m.insert("gg", "grid-gap");
    m.insert("gt", "grid-template");
    m.insert("gap", "gap");
    m
}

/// Check if text is a CSS shorthand
pub fn is_css_shorthand(text: &str) -> bool {
    let map = css_shorthand_map();
    for key in map.keys() {
        if text.starts_with(key) {
            let rest = &text[key.len()..];
            if !rest.is_empty() {
                // CSS shorthand values always start with a digit, #, or -.
                // This prevents single-letter keys like "d" (display) from
                // falsely matching HTML tag names like "div".
                let first_char = rest.chars().next().unwrap();
                if first_char.is_ascii_digit() || first_char == '#' || first_char == '-' {
                    return true;
                }
            }
        }
    }
    false
}

/// Expand CSS shorthand to full CSS property
pub fn expand_css_shorthand(shorthand: &str) -> String {
    let map = css_shorthand_map();

    for (prefix, property) in &map {
        if shorthand.starts_with(prefix) {
            let value = &shorthand[prefix.len()..];
            if value.is_empty() {
                return shorthand.to_string();
            }

            // Add px suffix for numeric values
            let css_value = if !value.ends_with("px") &&
                              !value.ends_with("em") &&
                              !value.ends_with("rem") &&
                              !value.ends_with('%') &&
                              !value.starts_with('#') &&
                              !value.contains(':') {
                if value.parse::<i32>().is_ok() {
                    format!("{}px", value)
                } else {
                    value.to_string()
                }
            } else {
                value.to_string()
            };

            return format!("{}: {};", property, css_value);
        }
    }

    shorthand.to_string()
}

/// Tag information parsed from Emmet abbreviation
#[derive(Clone, Debug)]
pub struct TagInfo {
    pub tag: String,
    pub id: String,
    pub classes: Vec<String>,
    pub attributes: HashMap<String, String>,
    pub text: String,
    pub repeat_count: usize,
}

/// Parse tag string like "div.container#id[attr=value]{text}"
pub fn parse_tag(tag_str: &str) -> TagInfo {
    let mut info = TagInfo {
        tag: "div".to_string(),
        id: String::new(),
        classes: Vec::new(),
        attributes: HashMap::new(),
        text: String::new(),
        repeat_count: 1,
    };

    let mut remaining = tag_str;

    // Extract tag name
    let tag_end = remaining.find(|c: char| !c.is_alphanumeric() && c != '-' && c != '_')
        .unwrap_or(remaining.len());
    if tag_end > 0 {
        info.tag = remaining[..tag_end].to_string();
        remaining = &remaining[tag_end..];
    }

    // Extract modifiers (#id, .class, [attr], {text}, *N) in any order.
    loop {
        if remaining.starts_with('#') {
            remaining = &remaining[1..];
            let id_end = remaining.find(|c: char| !c.is_alphanumeric() && c != '-' && c != '_')
                .unwrap_or(remaining.len());
            info.id = remaining[..id_end].to_string();
            remaining = &remaining[id_end..];
        } else if remaining.starts_with('.') {
            remaining = &remaining[1..];
            let class_end = remaining.find(|c: char| !c.is_alphanumeric() && c != '-' && c != '_')
                .unwrap_or(remaining.len());
            info.classes.push(remaining[..class_end].to_string());
            remaining = &remaining[class_end..];
        } else if remaining.starts_with('[') {
            remaining = &remaining[1..];
            let attr_end = remaining.find(']').unwrap_or(remaining.len());
            let attr_str = &remaining[..attr_end];
            remaining = &remaining[attr_end + 1..];

            if let Some(eq_pos) = attr_str.find('=') {
                let key = attr_str[..eq_pos].trim().to_string();
                let value = attr_str[eq_pos + 1..].trim().trim_matches('"').to_string();
                info.attributes.insert(key, value);
            } else {
                info.attributes.insert(attr_str.trim().to_string(), String::new());
            }
        } else if remaining.starts_with('{') {
            remaining = &remaining[1..];
            let text_end = remaining.find('}').unwrap_or(remaining.len());
            info.text = remaining[..text_end].to_string();
            remaining = &remaining[text_end + 1..];
        } else if remaining.starts_with('*') {
            remaining = &remaining[1..];
            let num_end = remaining.find(|c: char| !c.is_ascii_digit())
                .unwrap_or(remaining.len());
            if num_end > 0 {
                info.repeat_count = remaining[..num_end].parse().unwrap_or(1);
            }
        } else {
            break;
        }
    }

    info
}

/// A node in the parsed abbreviation tree (mirrors the C++ ParseNode).
#[derive(Clone, Debug, Default)]
pub struct ParseNode {
    pub tag: String,
    pub id: String,
    pub classes: Vec<String>,
    pub attributes: HashMap<String, String>,
    pub text: String,
    pub repeat_count: usize,
    pub children: Vec<ParseNode>,
}

/// Arena node used while parsing before converting to a nested tree.
struct RawNode {
    tag: String,
    id: String,
    classes: Vec<String>,
    attributes: HashMap<String, String>,
    text: String,
    repeat_count: usize,
    children: Vec<usize>,
}

/// Parse an abbreviation group into a nested node tree.
///
/// Operators: `>` child, `+` next token, `^` climb up, `*N` repeat,
/// `{text}` text content, plus plain tag tokens (with `#id`, `.class`,
/// `[attr=value]` handled by `parse_tag`).
fn parse_group(group: &str) -> ParseNode {
    fn empty_node(tag: &str) -> RawNode {
        RawNode {
            tag: tag.to_string(),
            id: String::new(),
            classes: Vec::new(),
            attributes: HashMap::new(),
            text: String::new(),
            repeat_count: 1,
            children: Vec::new(),
        }
    }

    let mut arena: Vec<RawNode> = vec![empty_node("root")];
    let mut stack: Vec<usize> = vec![0];
    let chars: Vec<char> = group.chars().collect();
    let mut i = 0;

    while i < chars.len() {
        match chars[i] {
            '>' | '+' => {
                // Both operators keep the current node as the parent of the
                // next token (matches the C++ parser's behavior).
                i += 1;
            }
            '^' => {
                // Climb up: move to the parent node.
                i += 1;
                if stack.len() > 1 {
                    stack.pop();
                }
            }
            '*' => {
                // Multiplication: set repeat count on the current node.
                i += 1;
                let mut num: usize = 0;
                while i < chars.len() && chars[i].is_ascii_digit() {
                    num = num * 10 + chars[i].to_digit(10).unwrap_or(0) as usize;
                    i += 1;
                }
                if num > 0 {
                    let cur = *stack.last().unwrap();
                    arena[cur].repeat_count = num;
                }
            }
            '{' => {
                // Text content.
                i += 1;
                let start = i;
                while i < chars.len() && chars[i] != '}' {
                    i += 1;
                }
                let cur = *stack.last().unwrap();
                arena[cur].text = chars[start..i].iter().collect();
                if i < chars.len() {
                    i += 1;
                }
            }
            _ => {
                // Parse a tag token up to the next operator.
                let start = i;
                while i < chars.len() && !matches!(chars[i], '>' | '+' | '^' | '*' | '{') {
                    i += 1;
                }
                let tag_str: String = chars[start..i].iter().collect();
                if !tag_str.is_empty() {
                    let info = parse_tag(&tag_str);
                    let idx = arena.len();
                    let cur = *stack.last().unwrap();
                    arena[cur].children.push(idx);
                    arena.push(RawNode {
                        tag: info.tag,
                        id: info.id,
                        classes: info.classes,
                        attributes: info.attributes,
                        text: info.text,
                        repeat_count: info.repeat_count,
                        children: Vec::new(),
                    });
                    stack.push(idx);
                }
            }
        }
    }

    fn convert(arena: &[RawNode], idx: usize) -> ParseNode {
        let raw = &arena[idx];
        ParseNode {
            tag: raw.tag.clone(),
            id: raw.id.clone(),
            classes: raw.classes.clone(),
            attributes: raw.attributes.clone(),
            text: raw.text.clone(),
            repeat_count: raw.repeat_count,
            children: raw.children.iter().map(|&c| convert(arena, c)).collect(),
        }
    }

    convert(&arena, 0)
}

/// Build the HTML attribute string from a node's id/classes/attributes.
fn node_attrs(node: &ParseNode) -> String {
    let mut attrs = String::new();
    if !node.id.is_empty() {
        attrs.push_str(&format!(" id=\"{}\"", node.id));
    }
    if !node.classes.is_empty() {
        attrs.push_str(&format!(" class=\"{}\"", node.classes.join(" ")));
    }
    for (key, value) in &node.attributes {
        if value.is_empty() {
            attrs.push_str(&format!(" {}", key));
        } else {
            attrs.push_str(&format!(" {}=\"{}\"", key, value));
        }
    }
    attrs
}

fn render_self_closing(tag: &str, attrs: &str) -> String {
    format!("<{}{} />", tag, attrs)
}

fn render_tag(tag: &str, attrs: &str, content: &str) -> String {
    format!("<{}{}>{}</{}>", tag, attrs, content, tag)
}

/// Render a parsed node tree into HTML (2-space indent per level).
fn render_node(node: &ParseNode, indent: usize) -> String {
    let prefix = " ".repeat(indent * 2);

    if node.tag == "root" {
        let mut result = String::new();
        for child in &node.children {
            result.push_str(&render_node(child, indent));
        }
        return result;
    }

    let attrs = node_attrs(node);
    let count = std::cmp::max(1, node.repeat_count);
    let mut result = String::new();

    for _ in 0..count {
        let mut content = String::new();
        for child in &node.children {
            content.push_str(&render_node(child, indent + 1));
        }

        if node.text.is_empty() && content.is_empty() && SELF_CLOSING_TAGS.contains(&node.tag.as_str()) {
            result.push_str(&prefix);
            result.push_str(&render_self_closing(&node.tag, &attrs));
            result.push('\n');
        } else {
            let mut inner = node.text.clone();
            if !content.is_empty() {
                if !inner.is_empty() {
                    inner.push('\n');
                }
                inner.push_str(&content);
                inner.push_str(&prefix);
            }
            result.push_str(&prefix);
            result.push_str(&render_tag(&node.tag, &attrs, &inner));
            result.push('\n');
        }
    }

    result
}

/// Expand Emmet abbreviation to HTML or CSS.
pub fn expand_emmet(abbreviation: &str) -> String {
    let abbr = abbreviation.trim();
    if abbr.is_empty() {
        return String::new();
    }

    // CSS shorthand
    if is_css_shorthand(abbr) {
        return expand_css_shorthand(abbr);
    }

    let root = parse_group(abbr);
    render_node(&root, 0).trim().to_string()
}

/// Check if a string looks like an Emmet abbreviation.
pub fn is_abbreviation(text: &str) -> bool {
    if text.is_empty() {
        return false;
    }
    let first = text.chars().next().unwrap();
    if !first.is_ascii_alphabetic() {
        return false;
    }
    text.chars().any(|c| matches!(c, '>' | '+' | '*' | '^' | '#' | '.' | '[' | ']' | '{' | '}' | ':' | '@'))
}

/// Extract the abbreviation ending at `cursor_pos` in `text`.
pub fn extract_abbreviation(text: &str, cursor_pos: usize) -> String {
    if cursor_pos == 0 || cursor_pos > text.len() {
        return String::new();
    }

    let chars: Vec<char> = text.chars().collect();
    let mut start = cursor_pos - 1;

    // Walk back to find the start of the abbreviation
    while start > 0 {
        let c = chars[start - 1];
        if c.is_alphanumeric()
            || matches!(c, '.' | '#' | '>' | '+' | '^' | '*' | '[' | ']' | '{' | '}' | '(' | ')' | '-' | ':' | '@')
        {
            start -= 1;
        } else {
            break;
        }
    }

    text[start..cursor_pos].to_string()
}

/// Expand abbreviation and return JSON for FFI
pub fn expand_emmet_json(abbreviation: &str) -> String {
    let expanded = expand_emmet(abbreviation);
    format!(r#"{{"expanded":"{}","isCSS":{}}}"#,
            escape_json(&expanded),
            is_css_shorthand(abbreviation))
}

/// Escape string for JSON
fn escape_json(s: &str) -> String {
    s.replace('\\', "\\\\")
     .replace('"', "\\\"")
     .replace('\n', "\\n")
     .replace('\r', "\\r")
     .replace('\t', "\\t")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_expand_div() {
        let result = expand_emmet("div");
        assert!(result.contains("<div>"));
        assert!(result.contains("</div>"));
    }

    #[test]
    fn test_css_shorthand() {
        assert!(is_css_shorthand("m10"));
        assert!(is_css_shorthand("p10-20"));
        assert!(is_css_shorthand("fs14"));
    }

    #[test]
    fn test_expand_css() {
        let result = expand_css_shorthand("m10");
        assert_eq!(result, "margin: 10px;");
    }

    // ── Ported from the C++ TestEmmetParser suite ──────────────

    #[test]
    fn test_expand_basic_tag() {
        assert_eq!(expand_emmet("span"), "<span></span>");
    }

    #[test]
    fn test_expand_nesting() {
        let out = expand_emmet("section>p");
        assert!(out.contains("<section>"));
        assert!(out.contains("<p></p>"));
        assert!(out.contains("</section>"));
    }

    #[test]
    fn test_expand_siblings() {
        let out = expand_emmet("span+em");
        assert_eq!(out.matches("<span>").count(), 1);
        assert_eq!(out.matches("<em>").count(), 1);
        assert!(out.find("<span>").unwrap() < out.find("<em>").unwrap());
    }

    #[test]
    fn test_expand_multiplication() {
        let out = expand_emmet("li*3");
        assert_eq!(out.matches("<li></li>").count(), 3);
    }

    #[test]
    fn test_expand_classes_and_id() {
        let out = expand_emmet("span.container#main");
        assert!(out.contains("id=\"main\""));
        assert!(out.contains("class=\"container\""));
        assert!(out.contains("<span"));
    }

    #[test]
    fn test_expand_text_content() {
        let out = expand_emmet("span{Hello}");
        assert!(out.contains("Hello"));
        assert!(out.contains("</span>"));
    }

    #[test]
    fn test_expand_attributes() {
        let out = expand_emmet("span[data-id=5]");
        assert!(out.contains("data-id=\"5\""));
    }

    #[test]
    fn test_expand_self_closing() {
        let out = expand_emmet("img");
        assert!(out.contains("<img"));
        assert!(out.contains("/>"));
        assert!(!out.contains("</img>"));
    }

    #[test]
    fn test_expand_empty() {
        assert!(expand_emmet("").is_empty());
        assert!(expand_emmet("   ").is_empty());
    }

    #[test]
    fn test_is_abbreviation() {
        assert!(is_abbreviation("span>p"));
        assert!(is_abbreviation("ul>li*3"));
        assert!(!is_abbreviation(""));
        assert!(!is_abbreviation("plain text"));
    }

    #[test]
    fn test_extract_abbreviation() {
        let text = "  div>p";
        // cursor at end => whole abbreviation
        assert_eq!(extract_abbreviation(text, 7), "div>p");
        // cursor before 'p' => only "div>"
        assert_eq!(extract_abbreviation(text, 6), "div>");
        assert!(extract_abbreviation("", 0).is_empty());
    }

    #[test]
    fn test_css_shorthand_expansion() {
        assert_eq!(expand_emmet("m1"), "margin: 1px;");
        assert_eq!(expand_emmet("p1"), "padding: 1px;");
        assert_eq!(expand_emmet("fs1"), "font-size: 1px;");
    }

    #[test]
    fn test_is_css_shorthand_values() {
        assert!(is_css_shorthand("m1"));
        assert!(is_css_shorthand("p1"));
        assert!(is_css_shorthand("fs1"));
        assert!(is_css_shorthand("bgc#fff"));
        assert!(is_css_shorthand("c#fff"));
        // Unlike the old C++ parser, multi-digit values are valid shorthands
        // ("m10" expands to "margin: 10px;").
        assert!(is_css_shorthand("m10"));
        assert!(is_css_shorthand("p20-10"));
        assert!(is_css_shorthand("fs14"));
        assert!(!is_css_shorthand(""));
    }

    #[test]
    fn test_parse_tag_info() {
        let info = parse_tag("a.link#home[href=/x]{Go}");
        assert_eq!(info.tag, "a");
        assert_eq!(info.id, "home");
        assert!(info.classes.contains(&"link".to_string()));
        assert_eq!(info.attributes.get("href").map(|s| s.as_str()), Some("/x"));
        assert_eq!(info.text, "Go");

        // Default tag
        let def = parse_tag(".box");
        assert_eq!(def.tag, "div");
    }

    #[test]
    fn test_expand_css_no_match() {
        assert_eq!(expand_css_shorthand("xyzzy"), "xyzzy");
    }

    #[test]
    fn test_expand_css_px_suffix() {
        assert_eq!(expand_css_shorthand("m10"), "margin: 10px;");
        assert_eq!(expand_css_shorthand("c#fff"), "color: #fff;");
    }

    #[test]
    fn test_expand_css_units() {
        // Values with explicit units must not get a px suffix
        assert_eq!(expand_css_shorthand("fs1em"), "font-size: 1em;");
        assert_eq!(expand_css_shorthand("fs100%"), "font-size: 100%;");
    }
}
