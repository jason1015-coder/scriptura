/// Emmet abbreviation parser and expander for HTML/CSS
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
    
    // Extract ID (#id)
    while remaining.starts_with('#') {
        remaining = &remaining[1..];
        let id_end = remaining.find(|c: char| !c.is_alphanumeric() && c != '-' && c != '_')
            .unwrap_or(remaining.len());
        info.id = remaining[..id_end].to_string();
        remaining = &remaining[id_end..];
    }
    
    // Extract classes (.class)
    while remaining.starts_with('.') {
        remaining = &remaining[1..];
        let class_end = remaining.find(|c: char| !c.is_alphanumeric() && c != '-' && c != '_')
            .unwrap_or(remaining.len());
        info.classes.push(remaining[..class_end].to_string());
        remaining = &remaining[class_end..];
    }
    
    // Extract attributes ([attr=value])
    while remaining.starts_with('[') {
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
    }
    
    // Extract text ({text})
    if remaining.starts_with('{') {
        remaining = &remaining[1..];
        let text_end = remaining.find('}').unwrap_or(remaining.len());
        info.text = remaining[..text_end].to_string();
        remaining = &remaining[text_end + 1..];
    }
    
    // Extract repeat count (*N)
    if remaining.starts_with('*') {
        remaining = &remaining[1..];
        let num_end = remaining.find(|c: char| !c.is_ascii_digit())
            .unwrap_or(remaining.len());
        if num_end > 0 {
            info.repeat_count = remaining[..num_end].parse().unwrap_or(1);
        }
    }
    
    info
}

/// Build HTML attributes string from tag info
fn build_attributes(info: &TagInfo) -> String {
    let mut attrs = String::new();
    
    if !info.id.is_empty() {
        attrs.push_str(&format!(" id=\"{}\"", info.id));
    }
    
    if !info.classes.is_empty() {
        attrs.push_str(&format!(" class=\"{}\"", info.classes.join(" ")));
    }
    
    for (key, value) in &info.attributes {
        if value.is_empty() {
            attrs.push_str(&format!(" {}", key));
        } else {
            attrs.push_str(&format!(" {}=\"{}\"", key, value));
        }
    }
    
    attrs
}

/// Render a single HTML tag
fn render_tag(info: &TagInfo, content: &str) -> String {
    let attrs = build_attributes(info);
    
    if info.text.is_empty() && content.is_empty() && SELF_CLOSING_TAGS.contains(&info.tag.as_str()) {
        format!("<{}{} />", info.tag, attrs)
    } else {
        let inner = if !info.text.is_empty() && !content.is_empty() {
            format!("{}\n{}", info.text, content)
        } else if !info.text.is_empty() {
            info.text.clone()
        } else {
            content.to_string()
        };
        format!("<{}{}>{}</{}>", info.tag, attrs, inner, info.tag)
    }
}

/// Expand Emmet abbreviation to HTML
pub fn expand_emmet(abbreviation: &str) -> String {
    let abbr = abbreviation.trim();
    if abbr.is_empty() {
        return String::new();
    }
    
    // CSS shorthand
    if is_css_shorthand(abbr) {
        return expand_css_shorthand(abbr);
    }
    
    // Simple tag expansion (simplified - full parser would handle nesting, siblings, etc.)
    let info = parse_tag(abbr);
    let count = std::cmp::max(1, info.repeat_count);
    let mut result = String::new();
    
    for i in 0..count {
        if i > 0 { result.push('\n'); }
        result.push_str(&render_tag(&info, ""));
    }
    
    result
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
}
