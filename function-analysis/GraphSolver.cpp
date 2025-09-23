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
    std::string outputRoundingMode = "Not Set";
    std::vector<json> orderedFunctionCalls; // For full node analysis - ordered calls with duplicates
    std::string location = ""; // Function location/file path
    bool isInTargetDirectory = false; // Whether function is in dReal-clang-tidy/dReal-CMake/src/
};

// Internal data structures
class GraphSolver {
private:
    std::map<std::string, FunctionNode> funcGraph; // FuncGraph
    std::map<std::string, FunctionNode*> funcDictionary; // FuncDictionary - pointers to nodes
    std::queue<std::string> funcQueue; // FuncQueue - function names ready to be processed
    std::set<std::string> initialSolvedFunctions; // Primary functions that are pre-solved

    // Helper function to check if a function is in the target directory
    bool isInTargetDirectory(const std::string& location) {
        if (location.empty()) return false;

        // If location just shows file name (no path), it's not even in dReal-clang
        if (location.find('/') == std::string::npos) {
            return false;
        }

        // Check if the location contains the target path
        return location.find("dReal-clang-tidy/dReal-CMake/src/") != std::string::npos;
    }

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

        int errorCount = 0;

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

            // Extract location if available
            if (func.contains("location")) {
                node.location = func["location"];
                node.isInTargetDirectory = isInTargetDirectory(node.location);
            }

            node.preRequirement = func.value("pre-requirement", "Not Set");
            node.outputRoundingMode = func.value("output-rounding-mode", "Not Set");
            node.error = func.value("error", "No");
            node.solved = true;

            // Count errors
            if (node.error != "No") {
                errorCount++;
            }
        }

        // Only print the count of primary nodes with errors
        std::cout << "Primary functions with errors: " << errorCount << std::endl;
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

            // Extract location if available
            if (func.contains("location")) {
                node.location = func["location"];
                node.isInTargetDirectory = isInTargetDirectory(node.location);
            }
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

            // Extract location if available
            if (func.contains("location")) {
                node.location = func["location"];
                node.isInTargetDirectory = isInTargetDirectory(node.location);
            }

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

    // Apply contradiction logic for nodes that call other functions
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
        // Only print node information if there's an error
        if (node.error != "No") {
            std::cout << "Solved node with ERROR: " << node.functionName
                      << " (pre-req: " << node.preRequirement
                      << ", output: " << node.outputRoundingMode
                      << ", error: " << node.error
                      << ", location: " << node.location << ")" << std::endl;
        }
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

    // Output results to JSON file with separated categories
    void outputResults(const std::string& filename) {
        // Separate categories for filtered output
        json targetDirErrors = json::array();
        json targetDirUnsolved = json::array();
        json targetDirSolved = json::array();
        json primaryErrors = json::array();
        json primaryUnsolved = json::array();
        json primarySolved = json::array();

        // Categories for full dump organized by type
        json targetDirDumpErrors = json::array();
        json targetDirDumpUnsolved = json::array();
        json targetDirDumpSolved = json::array();
        json primaryDumpErrors = json::array();
        json primaryDumpUnsolved = json::array();
        json primaryDumpSolved = json::array();
        json neitherDumpErrors = json::array();
        json neitherDumpUnsolved = json::array();
        json neitherDumpSolved = json::array();

        for (const auto& pair : funcGraph) {
            const FunctionNode& node = pair.second;
            bool isPrimary = initialSolvedFunctions.count(node.functionName) > 0;

            json nodeJson;
            nodeJson["function"] = node.functionName;
            nodeJson["solved"] = node.solved;
            nodeJson["pre-requirement"] = node.preRequirement;
            nodeJson["output-rounding-mode"] = node.outputRoundingMode;
            nodeJson["error"] = node.error;
            nodeJson["parents_count"] = node.parents.size();
            nodeJson["children_count"] = node.children.size();
            nodeJson["location"] = node.location;
            nodeJson["in_target_directory"] = node.isInTargetDirectory;
            nodeJson["is_primary"] = isPrimary;

            // Add parent details (name and location)
            json parentArray = json::array();
            for (const auto& parentName : node.parents) {
                json parentInfo;
                parentInfo["name"] = parentName;

                // Get parent location if available
                if (funcDictionary.find(parentName) != funcDictionary.end()) {
                    parentInfo["location"] = funcDictionary[parentName]->location;
                } else {
                    parentInfo["location"] = "Unknown";
                }

                parentArray.push_back(parentInfo);
            }
            nodeJson["parents"] = parentArray;

            json childArray = json::array();
            for (const auto& child : node.children) {
                childArray.push_back(child);
            }
            nodeJson["children"] = childArray;

            // Determine node category and status for full dump
            bool hasError = (node.error != "No");
            bool isSolved = node.solved;

            if (isPrimary) {
                // Primary node
                if (hasError) {
                    primaryErrors.push_back(nodeJson);
                    primaryDumpErrors.push_back(nodeJson);
                } else if (!isSolved) {
                    primaryUnsolved.push_back(nodeJson);
                    primaryDumpUnsolved.push_back(nodeJson);
                } else {
                    // Primary solved without error
                    primaryDumpSolved.push_back(nodeJson);
                }
            } else if (node.isInTargetDirectory) {
                // Target directory node
                if (hasError) {
                    targetDirErrors.push_back(nodeJson);
                    targetDirDumpErrors.push_back(nodeJson);
                } else if (!isSolved) {
                    targetDirUnsolved.push_back(nodeJson);
                    targetDirDumpUnsolved.push_back(nodeJson);
                } else {
                    targetDirSolved.push_back(nodeJson);
                    targetDirDumpSolved.push_back(nodeJson);
                }
            } else {
                // Neither primary nor in target directory
                if (hasError) {
                    neitherDumpErrors.push_back(nodeJson);
                } else if (!isSolved) {
                    neitherDumpUnsolved.push_back(nodeJson);
                } else {
                    neitherDumpSolved.push_back(nodeJson);
                }
            }
        }

        // Create the filtered results structure (same as before)
        json filteredResults;
        filteredResults["target_directory_nodes"] = {
            {"errors", targetDirErrors},
            {"unsolved", targetDirUnsolved},
            {"solved", targetDirSolved}
        };
        filteredResults["primary_nodes"] = {
            {"errors", primaryErrors},
            {"unsolved", primaryUnsolved}
        };

        // Create the full dump structure organized by category and status
        json fullDumpStructured;
        fullDumpStructured["target_directory_nodes"] = {
            {"errors", targetDirDumpErrors},
            {"unsolved", targetDirDumpUnsolved},
            {"solved", targetDirDumpSolved}
        };
        fullDumpStructured["primary_nodes"] = {
            {"errors", primaryDumpErrors},
            {"unsolved", primaryDumpUnsolved},
            {"solved", primaryDumpSolved}
        };
        fullDumpStructured["neither_nodes"] = {
            {"errors", neitherDumpErrors},
            {"unsolved", neitherDumpUnsolved},
            {"solved", neitherDumpSolved}
        };

        // Output filtered results
        std::ofstream outFile(filename);
        outFile << filteredResults.dump(4);
        outFile.close();

        // Output structured full dump
        std::string dumpFilename = filename.substr(0, filename.find_last_of('.')) + "Dump.json";
        std::ofstream dumpFile(dumpFilename);
        dumpFile << fullDumpStructured.dump(4);
        dumpFile.close();

        std::cout << "Filtered results (separated by category) written to " << filename << std::endl;
        std::cout << "Full dump (structured by category and status) written to " << dumpFilename << std::endl;

        // Print counts for each category
        std::cout << "\n=== Filtered Results Summary ===" << std::endl;
        std::cout << "Target directory nodes with errors: " << targetDirErrors.size() << std::endl;
        std::cout << "Target directory nodes unsolved: " << targetDirUnsolved.size() << std::endl;
        std::cout << "Target directory nodes solved: " << targetDirSolved.size() << std::endl;
        std::cout << "Primary nodes with errors: " << primaryErrors.size() << std::endl;
        std::cout << "Primary nodes unsolved: " << primaryUnsolved.size() << std::endl;

        std::cout << "\n=== Full Dump Summary ===" << std::endl;
        std::cout << "Target directory - Errors: " << targetDirDumpErrors.size()
                  << ", Unsolved: " << targetDirDumpUnsolved.size()
                  << ", Solved: " << targetDirDumpSolved.size() << std::endl;
        std::cout << "Primary nodes - Errors: " << primaryDumpErrors.size()
                  << ", Unsolved: " << primaryDumpUnsolved.size()
                  << ", Solved: " << primaryDumpSolved.size() << std::endl;
        std::cout << "Neither category - Errors: " << neitherDumpErrors.size()
                  << ", Unsolved: " << neitherDumpUnsolved.size()
                  << ", Solved: " << neitherDumpSolved.size() << std::endl;
    }
    // Print statistics with separate categories
    void printStatistics() {
        int totalNodes = funcGraph.size();

        // Target directory statistics
        int targetDirNodes = 0;
        int targetDirSolved = 0;
        int targetDirErrors = 0;
        int targetDirUnsolved = 0;

        // Primary node statistics
        int primaryNodes = 0;
        int primarySolved = 0;
        int primaryErrors = 0;
        int primaryUnsolved = 0;

        // Count different categories
        for (const auto& pair : funcGraph) {
            const FunctionNode& node = pair.second;
            bool isPrimary = initialSolvedFunctions.count(node.functionName) > 0;

            // Count target directory nodes (excluding primary)
            if (node.isInTargetDirectory && !isPrimary) {
                targetDirNodes++;
                if (node.solved) targetDirSolved++;
                if (node.error != "No") targetDirErrors++;
                if (!node.solved) targetDirUnsolved++;
            }

            // Count primary nodes
            if (isPrimary) {
                primaryNodes++;
                if (node.solved) primarySolved++;
                if (node.error != "No") primaryErrors++;
                if (!node.solved) primaryUnsolved++;
            }
        }

        std::cout << "\n=== Graph Statistics ===" << std::endl;
        std::cout << "Total nodes: " << totalNodes << std::endl;

        std::cout << "\n--- Target Directory Nodes (excluding primary) ---" << std::endl;
        std::cout << "Total target directory nodes: " << targetDirNodes << std::endl;
        std::cout << "Solved target directory nodes: " << targetDirSolved << std::endl;
        std::cout << "Error target directory nodes: " << targetDirErrors << std::endl;
        std::cout << "Unsolved target directory nodes: " << targetDirUnsolved << std::endl;

        std::cout << "\n--- Primary Nodes ---" << std::endl;
        std::cout << "Total primary nodes: " << primaryNodes << std::endl;
        std::cout << "Solved primary nodes: " << primarySolved << std::endl;
        std::cout << "Error primary nodes: " << primaryErrors << std::endl;
        std::cout << "Unsolved primary nodes: " << primaryUnsolved << std::endl;
    }
};

int main() {
    GraphSolver solver;

    std::cout << "\n \nLoading primary functions..." << std::endl;
    solver.loadPrimaryFunctions("PrimaryFunctions.json");

    std::cout << "Loading full functions..." << std::endl;
    solver.loadFunctionsFull("functionsFull.json");

    std::cout << "Loading parent list and building graph..." << std::endl;
    solver.loadFunctionsParentList("functionsParentList.json");

    //solving graph
    solver.solve();

    std::cout << "\nOutputting results..." << std::endl;
    solver.outputResults("GraphSolverResults.json");

    solver.printStatistics();

    return 0;
}