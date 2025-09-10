#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <queue>
#include "json.hpp"

using json = nlohmann::ordered_json;

// Node properties for each function in the graph
struct FunctionNode {
    std::string functionName;
    bool solved = false;
    std::vector<std::string> parents; // Functions called by this function
    std::vector<std::string> children; // Functions that call this function
    std::string error = "No";
    std::string preRequirement = "Not Set";
    std::vector<std::string> incomingRoundingModes; // Observed incoming rounding modes
    std::string outputRoundingMode = "Not Set";
    std::vector<json> orderedFunctionCalls; // For full node analysis - ordered calls with duplicates
};

// Internal data structures
class GraphSolver {
private:
    std::map<std::string, FunctionNode> funcGraph; // FuncGraph
    std::map<std::string, FunctionNode*> funcDictionary; // FuncDictionary - pointers to nodes
    std::queue<std::string> funcQueue; // FuncQueue - function names ready to be processed
    std::set<std::string> initialSolvedFunctions; // Primary functions that are pre-solved

public:
    // Load primary functions and mark them as initially solved
    void loadPrimaryFunctions(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filename << std::endl;
            return;
        }

        json primaryData;
        file >> primaryData;
        file.close();

        for (const auto& func : primaryData) {
            std::string funcName = func["function"];
            initialSolvedFunctions.insert(funcName);

            // Create or update node for primary function
            if (funcGraph.find(funcName) == funcGraph.end()) {
                funcGraph[funcName] = FunctionNode();
                funcGraph[funcName].functionName = funcName;
                funcDictionary[funcName] = &funcGraph[funcName];
            }

            FunctionNode& node = funcGraph[funcName];
            node.preRequirement = func.value("pre-requirement", "Not Set");
            node.outputRoundingMode = func.value("output-rounding-mode", "Not Set");
            node.error = func.value("error", "No");
            node.solved = true;

            std::cout << "Loaded primary function: " << funcName
                      << " (pre-req: " << node.preRequirement
                      << ", output: " << node.outputRoundingMode << ")" << std::endl;
        }
    }

    // Load full function data with ordered calls
    void loadFunctionsFull(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filename << std::endl;
            return;
        }

        json fullData;
        file >> fullData;
        file.close();

        for (const auto& func : fullData) {
            std::string funcName = func["function"];

            // Create or update node
            if (funcGraph.find(funcName) == funcGraph.end()) {
                funcGraph[funcName] = FunctionNode();
                funcGraph[funcName].functionName = funcName;
                funcDictionary[funcName] = &funcGraph[funcName];
            }

            FunctionNode& node = funcGraph[funcName];
            node.orderedFunctionCalls = func["function_calls"];
        }
    }

    // Load parent list and build the graph structure
    void loadFunctionsParentList(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filename << std::endl;
            return;
        }

        json parentData;
        file >> parentData;
        file.close();

        // First pass: Create all nodes and set up parent relationships
        for (const auto& func : parentData) {
            std::string funcName = func["function"];

            // Create or update node
            if (funcGraph.find(funcName) == funcGraph.end()) {
                funcGraph[funcName] = FunctionNode();
                funcGraph[funcName].functionName = funcName;
                funcDictionary[funcName] = &funcGraph[funcName];
            }

            FunctionNode& node = funcGraph[funcName];

            // Set up parents
            for (const auto& parent : func["unique_function_calls"]) {
                std::string parentName = parent;
                node.parents.push_back(parentName);

                // Create parent node if it doesn't exist
                if (funcGraph.find(parentName) == funcGraph.end()) {
                    funcGraph[parentName] = FunctionNode();
                    funcGraph[parentName].functionName = parentName;
                    funcDictionary[parentName] = &funcGraph[parentName];
                }
            }
        }

        // Second pass: Set up children relationships
        for (auto& pair : funcGraph) {
            FunctionNode& node = pair.second;
            for (const std::string& parentName : node.parents) {
                if (funcDictionary.find(parentName) != funcDictionary.end()) {
                    funcDictionary[parentName]->children.push_back(node.functionName);
                }
            }
        }

        // Third pass: Add nodes to queue if they have no parents or are initially solved
        for (auto& pair : funcGraph) {
            FunctionNode& node = pair.second;
            if (node.parents.empty() || initialSolvedFunctions.count(node.functionName)) {
                if (!node.solved && initialSolvedFunctions.count(node.functionName)) {
                    node.solved = true; // Mark as solved if it's a primary function
                }
                funcQueue.push(node.functionName);
                std::cout << "Added to queue: " << node.functionName
                          << " (parents: " << node.parents.size()
                          << ", initially solved: " << (initialSolvedFunctions.count(node.functionName) ? "yes" : "no")
                          << ")" << std::endl;
            }
        }
    }

    // Check if all parents of a node are solved
    bool allParentsSolved(const FunctionNode& node) {
        for (const std::string& parentName : node.parents) {
            if (funcDictionary.find(parentName) != funcDictionary.end()) {
                if (!funcDictionary[parentName]->solved) {
                    return false;
                }
            }
        }
        return true;
    }

    // Apply contradiction logic similar to first-pass for nodes that call other functions
    void solveNode(FunctionNode& node) {
        if (node.solved || !node.orderedFunctionCalls.empty()) {
            // This is a non-primary node that calls other functions
            // Apply logic based on the rounding modes of called functions

            int requiredRoundingMode = 2; // 0=nearest, 1=upper, 2=not set
            int currentRoundingMode = 2;
            bool requiredSet = false;
            bool errorOccurred = false;
            std::string errorMessage = "No";

            // Process ordered function calls to determine rounding requirements
            for (const auto& call : node.orderedFunctionCalls) {
                std::string calledFunc = call["called_function"];

                if (funcDictionary.find(calledFunc) != funcDictionary.end()) {
                    FunctionNode& calledNode = *funcDictionary[calledFunc];

                    if (calledNode.solved && calledNode.error == "No") {
                        // Determine what rounding mode this function call requires
                        std::string calledPreReq = calledNode.preRequirement;
                        std::string calledOutput = calledNode.outputRoundingMode;

                        // If the called function requires a specific rounding mode
                        if (calledPreReq == "Upper") {
                            if (currentRoundingMode == 2) { // Not set
                                if (!requiredSet) {
                                    requiredRoundingMode = 1; // Upper
                                    requiredSet = true;
                                }
                                currentRoundingMode = 1;
                            } else if (currentRoundingMode == 0) { // Nearest
                                errorMessage = "Function " + calledFunc + " requires Upper rounding but current mode is Nearest";
                                errorOccurred = true;
                                break;
                            }
                            // If already Upper (1), stay Upper
                        } else if (calledPreReq == "Nearest") {
                            if (currentRoundingMode == 2) { // Not set
                                if (!requiredSet) {
                                    requiredRoundingMode = 0; // Nearest
                                    requiredSet = true;
                                }
                                currentRoundingMode = 0;
                            } else if (currentRoundingMode == 1) { // Upper
                                errorMessage = "Function " + calledFunc + " requires Nearest rounding but current mode is Upper";
                                errorOccurred = true;
                                break;
                            }
                            // If already Nearest (0), stay Nearest
                        }

                        // Update current rounding mode based on called function's output
                        if (calledOutput == "Upper") {
                            currentRoundingMode = 1;
                        } else if (calledOutput == "Nearest") {
                            currentRoundingMode = 0;
                        }
                    }
                }
            }

            if (errorOccurred) {
                node.error = errorMessage;
                node.preRequirement = "Error";
                node.outputRoundingMode = "Error";
            } else {
                node.preRequirement = roundingModeToString(requiredRoundingMode);
                node.outputRoundingMode = roundingModeToString(currentRoundingMode);
                node.error = "No";
            }
        }

        node.solved = true;
        std::cout << "Solved node: " << node.functionName
                  << " (pre-req: " << node.preRequirement
                  << ", output: " << node.outputRoundingMode
                  << ", error: " << node.error << ")" << std::endl;
    }

    // Helper function to convert rounding mode integer to string
    std::string roundingModeToString(int mode) {
        switch(mode) {
            case 0: return "Nearest";
            case 1: return "Upper";
            case 2: return "Not Set";
            default: return "Unknown";
        }
    }

    // Add children of a solved node to the queue if all their parents are solved
    void addChildrenToQueue(const FunctionNode& solvedNode) {
        for (const std::string& childName : solvedNode.children) {
            if (funcDictionary.find(childName) != funcDictionary.end()) {
                FunctionNode& child = *funcDictionary[childName];
                if (!child.solved && allParentsSolved(child)) {
                    funcQueue.push(childName);
                    std::cout << "Added child to queue: " << childName << std::endl;
                }
            }
        }
    }

    // Main solving algorithm
    void solve() {
        std::cout << "\nStarting graph solving process..." << std::endl;
        std::cout << "Queue size: " << funcQueue.size() << std::endl;

        int processedCount = 0;
        while (!funcQueue.empty()) {
            std::string currentFunc = funcQueue.front();
            funcQueue.pop();

            if (funcDictionary.find(currentFunc) != funcDictionary.end()) {
                FunctionNode& node = *funcDictionary[currentFunc];

                if (!node.solved) {
                    solveNode(node);
                    processedCount++;
                }

                // Add children to queue if they're ready
                addChildrenToQueue(node);
            }
        }

        std::cout << "Solving complete. Processed " << processedCount << " nodes." << std::endl;
    }

    // Output results to JSON file
    void outputResults(const std::string& filename) {
        json results = json::array();

        for (const auto& pair : funcGraph) {
            const FunctionNode& node = pair.second;

            json nodeJson;
            nodeJson["function"] = node.functionName;
            nodeJson["solved"] = node.solved;
            nodeJson["pre-requirement"] = node.preRequirement;
            nodeJson["output-rounding-mode"] = node.outputRoundingMode;
            nodeJson["error"] = node.error;
            nodeJson["parents_count"] = node.parents.size();
            nodeJson["children_count"] = node.children.size();

            // Add parent and children lists for debugging
            json parentArray = json::array();
            for (const auto& parent : node.parents) {
                parentArray.push_back(parent);
            }
            nodeJson["parents"] = parentArray;

            json childArray = json::array();
            for (const auto& child : node.children) {
                childArray.push_back(child);
            }
            nodeJson["children"] = childArray;

            results.push_back(nodeJson);
        }

        std::ofstream outFile(filename);
        outFile << results.dump(4);
        outFile.close();

        std::cout << "Results written to " << filename << std::endl;
    }

    // Print statistics
    void printStatistics() {
        int totalNodes = funcGraph.size();
        int solvedNodes = 0;
        int errorNodes = 0;
        int primaryNodes = initialSolvedFunctions.size();

        for (const auto& pair : funcGraph) {
            if (pair.second.solved) solvedNodes++;
            if (pair.second.error != "No") errorNodes++;
        }

        std::cout << "\n=== Graph Statistics ===" << std::endl;
        std::cout << "Total nodes: " << totalNodes << std::endl;
        std::cout << "Primary nodes: " << primaryNodes << std::endl;
        std::cout << "Solved nodes: " << solvedNodes << std::endl;
        std::cout << "Error nodes: " << errorNodes << std::endl;
        std::cout << "Unsolved nodes: " << (totalNodes - solvedNodes) << std::endl;
    }
};

int main() {
    GraphSolver solver;

    std::cout << "Loading primary functions..." << std::endl;
    solver.loadPrimaryFunctions("PrimaryFunctions.json");

    std::cout << "\nLoading full functions..." << std::endl;
    solver.loadFunctionsFull("functionsFull.json");

    std::cout << "\nLoading parent list and building graph..." << std::endl;
    solver.loadFunctionsParentList("functionsParentList.json");

    std::cout << "\nSolving graph..." << std::endl;
    solver.solve();

    std::cout << "\nOutputting results..." << std::endl;
    solver.outputResults("GraphSolverResults.json");

    solver.printStatistics();

    return 0;
}