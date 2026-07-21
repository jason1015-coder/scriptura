//! ## Dependency Resolver
//!
//! Handles plugin dependency resolution using topological sort.
//! Replaces `DependencyResolver` from the original C++ codebase.
//!
//! Plugins declare their dependencies in their metadata JSON.
//! The resolver builds a dependency graph and sorts plugins so that
//! dependencies are loaded before their dependents.

use std::collections::{HashMap, HashSet, VecDeque};

use serde_json::Value;

#[derive(Default)]
pub struct DependencyResolver {
    plugins: HashMap<String, Vec<String>>, // id -> [dependency_ids]
    metadata: HashMap<String, String>,     // id -> JSON string
}

impl DependencyResolver {
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a plugin and its dependencies from a metadata JSON string.
    pub fn add_plugin(&mut self, id: &str, metadata_json: &str) -> Result<(), String> {
        let metadata: Value = serde_json::from_str(metadata_json)
            .map_err(|e| format!("Invalid metadata JSON for '{}': {}", id, e))?;

        let deps = metadata
            .get("dependencies")
            .and_then(|d| d.as_array())
            .map(|arr| {
                arr.iter()
                    .filter_map(|v| v.as_str().map(String::from))
                    .collect()
            })
            .unwrap_or_default();

        self.plugins.insert(id.to_string(), deps);
        self.metadata.insert(id.to_string(), metadata_json.to_string());
        Ok(())
    }

    /// Resolve all registered plugins and return a load order.
    /// Returns an error if a cycle is detected.
    pub fn resolve(&self) -> Result<Vec<String>, String> {
        let all_plugins: HashSet<&str> = self.plugins.keys().map(|s| s.as_str()).collect();

        // Kahn's algorithm for topological sort
        let mut in_degree: HashMap<&str, usize> = HashMap::new();
        let mut graph: HashMap<&str, Vec<&str>> = HashMap::new();

        for (id, deps) in &self.plugins {
            in_degree.entry(id.as_str()).or_insert(0);
            for dep in deps {
                if !all_plugins.contains(dep.as_str()) {
                    return Err(format!(
                        "Plugin '{}' depends on '{}' which is not registered",
                        id, dep
                    ));
                }
                graph.entry(dep.as_str()).or_default().push(id.as_str());
                *in_degree.entry(id.as_str()).or_insert(0) += 1;
            }
        }

        let mut queue: VecDeque<&str> = in_degree
            .iter()
            .filter(|(_, &deg)| deg == 0)
            .map(|(&id, _)| id)
            .collect();

        let mut sorted: Vec<String> = Vec::new();

        while let Some(node) = queue.pop_front() {
            sorted.push(node.to_string());
            if let Some(neighbors) = graph.get(node) {
                for &neighbor in neighbors {
                    if let Some(deg) = in_degree.get_mut(neighbor) {
                        *deg -= 1;
                        if *deg == 0 {
                            queue.push_back(neighbor);
                        }
                    }
                }
            }
        }

        if sorted.len() != self.plugins.len() {
            return Err("Circular dependency detected among plugins".to_string());
        }

        Ok(sorted)
    }

    /// Get the resolved load order (must call `resolve()` first).
    pub fn resolve_order(&self) -> Vec<String> {
        self.resolve().unwrap_or_default()
    }

    /// Check if the resolver has any plugins registered.
    pub fn is_empty(&self) -> bool {
        self.plugins.is_empty()
    }

    /// Get the number of registered plugins.
    pub fn len(&self) -> usize {
        self.plugins.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_plugin(id: &str, deps: &[&str]) -> String {
        let deps_json: Vec<String> = deps.iter().map(|d| format!("\"{}\"", d)).collect();
        format!(
            r#"{{"id":"{}","name":"{}","dependencies":[{}]}}"#,
            id,
            id,
            deps_json.join(",")
        )
    }

    fn make_plugin_no_deps(id: &str) -> String {
        format!(r#"{{"id":"{}","name":"{}"}}"#, id, id)
    }

    #[test]
    fn test_new_resolver() {
        let r = DependencyResolver::new();
        assert!(r.is_empty());
        assert_eq!(r.len(), 0);
    }

    #[test]
    fn test_single_plugin() {
        let mut r = DependencyResolver::new();
        r.add_plugin("plugin_a", &make_plugin_no_deps("plugin_a")).unwrap();
        assert!(!r.is_empty());
        assert_eq!(r.len(), 1);
        let order = r.resolve().unwrap();
        assert_eq!(order, vec!["plugin_a"]);
    }

    #[test]
    fn test_simple_dependency() {
        let mut r = DependencyResolver::new();
        r.add_plugin("plugin_b", &make_plugin("plugin_b", &["plugin_a"])).unwrap();
        r.add_plugin("plugin_a", &make_plugin_no_deps("plugin_a")).unwrap();
        let order = r.resolve().unwrap();
        assert_eq!(order, vec!["plugin_a", "plugin_b"]);
    }

    #[test]
    fn test_chain_dependency() {
        let mut r = DependencyResolver::new();
        r.add_plugin("a", &make_plugin("a", &[])).unwrap();
        r.add_plugin("b", &make_plugin("b", &["a"])).unwrap();
        r.add_plugin("c", &make_plugin("c", &["b"])).unwrap();
        let order = r.resolve().unwrap();
        assert_eq!(order, vec!["a", "b", "c"]);
    }

    #[test]
    fn test_no_dependencies_order() {
        let mut r = DependencyResolver::new();
        r.add_plugin("b", &make_plugin_no_deps("b")).unwrap();
        r.add_plugin("a", &make_plugin_no_deps("a")).unwrap();
        // With no dependencies, either order is valid
        let order = r.resolve().unwrap();
        assert_eq!(order.len(), 2);
        assert!(order.contains(&"a".to_string()));
        assert!(order.contains(&"b".to_string()));
    }

    #[test]
    fn test_missing_dependency() {
        let mut r = DependencyResolver::new();
        r.add_plugin("p", &make_plugin("p", &["missing"])).unwrap();
        let result = r.resolve();
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not registered"));
    }

    #[test]
    fn test_circular_dependency() {
        let mut r = DependencyResolver::new();
        r.add_plugin("a", &make_plugin("a", &["b"])).unwrap();
        r.add_plugin("b", &make_plugin("b", &["a"])).unwrap();
        let result = r.resolve();
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Circular dependency"));
    }

    #[test]
    fn test_self_dependency() {
        let mut r = DependencyResolver::new();
        r.add_plugin("a", &make_plugin("a", &["a"])).unwrap();
        let result = r.resolve();
        assert!(result.is_err());
    }

    #[test]
    fn test_invalid_metadata_json() {
        let mut r = DependencyResolver::new();
        let result = r.add_plugin("p", "not valid json");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Invalid metadata JSON"));
    }

    #[test]
    fn test_multiple_dependents() {
        let mut r = DependencyResolver::new();
        r.add_plugin("base", &make_plugin_no_deps("base")).unwrap();
        r.add_plugin("ext1", &make_plugin("ext1", &["base"])).unwrap();
        r.add_plugin("ext2", &make_plugin("ext2", &["base"])).unwrap();
        let order = r.resolve().unwrap();
        assert_eq!(order[0], "base");
        assert_eq!(order.len(), 3);
    }

    #[test]
    fn test_resolve_order_convenience() {
        let mut r = DependencyResolver::new();
        r.add_plugin("a", &make_plugin_no_deps("a")).unwrap();
        // resolve_order returns empty vec on failure (no cycle for single plugin)
        let order = r.resolve_order();
        assert_eq!(order, vec!["a"]);
    }

    #[test]
    fn test_empty_resolver_resolve() {
        let r = DependencyResolver::new();
        let order = r.resolve().unwrap();
        assert!(order.is_empty());
    }
}
