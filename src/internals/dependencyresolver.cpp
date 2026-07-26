#include "dependencyresolver.h"
#include <QJsonArray>
#include <algorithm>
#include <QDebug>

QHash<QString, QStringList> DependencyResolver::buildGraph(const QList<QJsonObject>& plugins)
{
    QHash<QString, QStringList> graph;
    
    for (const QJsonObject& plugin : plugins) {
        QString id = plugin["id"].toString();
        if (id.isEmpty()) {
            continue;
        }
        
        QStringList deps;
        if (plugin.contains("dependencies")) {
            QJsonArray depArray = plugin["dependencies"].toArray();
            for (const QJsonValue& val : depArray) {
                deps.append(val.toString());
            }
        }
        
        graph[id] = deps;
    }
    
    return graph;
}

bool DependencyResolver::detectCycle(const QHash<QString, QStringList>& graph,
                                    const QString& node,
                                    QSet<QString>& visited,
                                    QSet<QString>& recStack)
{
    if (!graph.contains(node)) {
        return false;
    }
    
    if (recStack.contains(node)) {
        return true; // Found cycle
    }
    
    if (visited.contains(node)) {
        return false; // Already processed
    }
    
    visited.insert(node);
    recStack.insert(node);
    
    for (const QString& dep : graph[node]) {
        if (detectCycle(graph, dep, visited, recStack)) {
            return true;
        }
    }
    
    recStack.remove(node);
    return false;
}

bool DependencyResolver::hasCircularDependency(const QList<QJsonObject>& plugins)
{
    QHash<QString, QStringList> graph = buildGraph(plugins);
    QSet<QString> visited;
    QSet<QString> recStack;
    
    for (auto it = graph.constBegin(); it != graph.constEnd(); ++it) {
        if (detectCycle(graph, it.key(), visited, recStack)) {
            return true;
        }
    }
    
    return false;
}

QList<DependencyResolver::DependencyError> DependencyResolver::validate(const QList<QJsonObject>& plugins, const QSet<QString>& actuallyLoaded)
{
    QList<DependencyError> errors;
    QSet<QString> availablePlugins;
    
    // Collect all available plugin IDs
    for (const QJsonObject& plugin : plugins) {
        QString id = plugin["id"].toString();
        if (!id.isEmpty()) {
            availablePlugins.insert(id);
        }
    }
    
    // Check each plugin's dependencies
    for (const QJsonObject& plugin : plugins) {
        QString pluginId = plugin["id"].toString();
        if (pluginId.isEmpty()) {
            continue;
        }
        
        // Check required dependencies against actuallyLoaded
        if (plugin.contains("dependencies")) {
            QJsonArray depArray = plugin["dependencies"].toArray();
            for (const QJsonValue& val : depArray) {
                QString depId = val.toString();
                if (!actuallyLoaded.contains(depId)) {
                    errors.append(DependencyError(pluginId, depId, false));
                }
            }
        }
        
        // Check optional dependencies
        if (plugin.contains("optionalDependencies")) {
            QJsonArray optDepArray = plugin["optionalDependencies"].toArray();
            for (const QJsonValue& val : optDepArray) {
                QString depId = val.toString();
                if (!availablePlugins.contains(depId)) {
                    qInfo() << "Optional dependency not found:" << depId << "for plugin" << pluginId;
                }
            }
        }
    }
    
    return errors;
}

QStringList DependencyResolver::dfsSort(const QHash<QString, QStringList>& graph)
{
    QStringList result;
    QSet<QString> visited;
    QSet<QString> inResult;
    
    // Recursive visit function
    std::function<void(const QString&)> visit = [&](const QString& node) {
        if (inResult.contains(node) || !graph.contains(node)) {
            return;
        }
        
        visited.insert(node);
        
        for (const QString& dep : graph[node]) {
            if (!visited.contains(dep)) {
                visit(dep);
            }
        }
        
        inResult.insert(node);
        result.append(node);
    };
    
    // Visit all nodes
    for (auto it = graph.constBegin(); it != graph.constEnd(); ++it) {
        if (!visited.contains(it.key())) {
            visit(it.key());
        }
    }
    
    return result;
}

QStringList DependencyResolver::topologicalSort(const QList<QJsonObject>& plugins)
{
    QHash<QString, QStringList> graph = buildGraph(plugins);
    
    // Check for circular dependencies
    if (hasCircularDependency(plugins)) {
        qCritical() << "Circular dependency detected, cannot perform topological sort";
        return QStringList();
    }
    
    return dfsSort(graph);
}
