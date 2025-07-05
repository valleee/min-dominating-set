#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <vector>
#include <cstdint>
#include <regex>
#include <optional>
#include <assert.h>
#include <cmath>
#include <numeric>
#include <functional>
#include <limits>
#include <algorithm>
#include <unordered_set>

/************************************************************************************************************************/
/* Definition of Data-Structured used in the algorithm */
enum class BagType : char
{
    Forget = 'f', Intro = 'i', Join = 'j', Leaf = 'l'
};

enum class Color : std::uint8_t
{
    // a white vertex means, that in the current sub-problem it is not added to the dominating set
    // -> must be dominated in the partial solution
    White = 0,
    // a black vertex means, that in the current sub-problem it is added to the dominating set
    // -> must dominate all white vertices of the partial solution 
    Black = 1,
    // a grey vertex means, that this vertex is not part of the current solution
    // -> does not have to be dominated in the partial solution, but might be
    Grey = 2
};
constexpr Color colorArr[] = { Color::White, Color::Black, Color::Grey }; // use for easy iteration
constexpr std::tuple<Color, Color, Color> consistentColorsArr[] = {
    std::make_tuple(Color::Black, Color::Black, Color::Black),
    std::make_tuple(Color::White, Color::White, Color::Grey),
    std::make_tuple(Color::White, Color::Grey, Color::White),
    std::make_tuple(Color::Grey, Color::Grey, Color::Grey)
};

struct ColorPairHash
{
    std::size_t operator()(const std::shared_ptr<const std::pair<int, Color>>& key) const noexcept
    {
        return std::hash<int>{}(key.get()->first) ^ std::hash<Color>{}(key.get()->second);
    }
};

struct ColorPairEquality
{
    using is_transparent = void;  // Enable heterogeneous lookup

    bool operator()(
        const std::shared_ptr<const std::pair<int, Color>>& p1,
        const std::shared_ptr<const std::pair<int, Color>>& p2
    ) const {
        return p1.get()->first == p2.get()->first && p1.get()->second == p2.get()->second;
    }
    bool operator()(
        const std::pair<int, Color>& p1,
        const std::shared_ptr<const std::pair<int, Color>>& p2
    ) const {
        return p1.first == p2.get()->first && p1.second == p2.get()->second;
    }
    bool operator()(
        const std::shared_ptr<const std::pair<int, Color>>& p1,
        const std::pair<int, Color>& p2
    ) const {
        return p1.get()->first == p2.first && p1.get()->second == p2.second;
    }
};

// this data-structure will keep track of a pair: (vertex -> color) via shared_ptrs
// to avoid creating the same pair multiple times
std::unordered_set<
    std::shared_ptr<const std::pair<int, Color>>,
    ColorPairHash,
    ColorPairEquality
> colorPairSet = {};

std::shared_ptr<const std::pair<int, Color>> lookup(int vertex, Color color)
{
    const auto ptr = std::make_shared<std::pair<int, Color>>(vertex, color);
    const auto it = colorPairSet.find(ptr);
    if (it == colorPairSet.end())
    {
        colorPairSet.insert(ptr);
        return std::move(ptr);
    }
    else
    {
        return *it;
    }
}

// one function mapping a set of vertices to 3 colors 
using Coloring = std::vector<std::shared_ptr<const std::pair<int, Color>>>;

struct ColoringHash 
{
    std::size_t operator()(const Coloring& key) const noexcept
    {
        std::size_t hash = 0;
        
        for (const auto ptr : key)
        {
            const auto& [vertex, color] = *ptr.get(); 
            // commutative accumulation of hash, such that order of coloring-pairs does not influence the hash
            hash += std::hash<int>{}(vertex) ^ std::hash<Color>{}(color);
        }

        return hash;
    }
};

struct ColoringEquality
{
    bool operator()(
        const Coloring& key1,
        const Coloring& key2
    )
        const noexcept
    {
        if (key1.size() != key2.size())
        {
            return false;
        }
        else
        {
            for (const auto &pair1 : key1)
            {
                bool contains = false;
                for (const auto &pair2 : key2)
                {
                    if (pair1 == pair2)
                    {
                        contains = true;
                        break;
                    }
                }
                if (!contains)
                {
                    return false;
                }
            }
            return true;
        }
    }
};

struct Bag
{
    std::uint16_t number;
    BagType type;
    std::optional<std::uint16_t> parentNumber;
    std::vector<int> bagElements;
    std::vector<std::pair<int, int>> introduceEdges;
    std::optional<std::uint16_t> child1 = std::nullopt;
    std::optional<std::uint16_t> child2 = std::nullopt;

    // datastructure to map colorings (minimum compatible set) to values
    using HashMap = std::unordered_map<
        Coloring,           // key
        int,                // value
        ColoringHash,       // key-hashing-function (commutative)
        ColoringEquality    // key-equality (commutative)
    >;
    HashMap c;

    // used only for join nodes
    using ConsistentColorings = std::vector<std::tuple<Coloring, Coloring, Coloring>>;
    ConsistentColorings consistentColorings;

    explicit Bag(std::uint16_t number, BagType type, std::optional<std::uint16_t> parentNumber,
        std::vector<int> vertices, std::vector<std::pair<int, int>> edges) :
        number(number), type(type), parentNumber(parentNumber), bagElements(std::move(vertices)),
        introduceEdges(std::move(edges))
    {
        // only the root (reserved number 0) has not parent in a labelled TD
        assert(number == 0 || parentNumber.has_value());
        // root must be empty bag
        assert(number != 0 || bagElements.size() == 0);
        // introduce edges not possible on leaf-nodes
        assert(introduceEdges.size() == 0 || type != BagType::Leaf);

        // fill search state for this bag
        const auto bagsize = bagElements.size();
        if (bagsize > 0)
        {
            HashMap map;
            map.reserve(std::pow(3, bagsize));

            // builds all 3^bagsize states with backtracking
            std::function<void(Coloring&)> backtrack1 = [&backtrack1, &bagsize, &map, this](Coloring& current) -> void
            {
                if (current.size() == bagsize)
                {
                    map[current] = std::numeric_limits<int>::max();
                    return;
                }
                for (const auto &color : colorArr)
                {
                    const auto coloredVertex = bagElements[current.size()];
                    current.push_back(lookup(coloredVertex, color));
                    backtrack1(current);
                    current.pop_back();
                }
            };
            Coloring current;
            current.reserve(bagsize);
            backtrack1(current);
            this->c = std::move(map);

            // if this node is a join node compute all consistent colorings
            if (type == BagType::Join)
            {
                ConsistentColorings ccs;
                ccs.reserve(std::pow(4, bagsize));

                // builds all 4^bagsize consistent states with backtracking
                std::function<void(Coloring&, Coloring&, Coloring&)> backtrack2 = [&backtrack2, &ccs, &bagsize, this]
                    (Coloring& current1, Coloring& current2, Coloring& current3) -> void
                {
                    if (current1.size() == bagsize)
                    {
                        ccs.emplace_back(current1, current2, current3);
                        return;
                    }

                    for (const auto [c1,c2,c3] : consistentColorsArr)
                    {
                        const auto vertex = bagElements[current1.size()];
                        current1.push_back(lookup(vertex, c1));
                        current2.push_back(lookup(vertex, c2));
                        current3.push_back(lookup(vertex, c3));
                        backtrack2(current1, current2, current3);
                        current1.pop_back();
                        current2.pop_back();
                        current3.pop_back();
                    }
                };
                Coloring current1;
                Coloring current2;
                Coloring current3;
                current1.reserve(bagElements.size());
                current2.reserve(bagElements.size());
                current3.reserve(bagElements.size());
                backtrack2(current1, current2, current3);
                this->consistentColorings = std::move(ccs);
            }
        }
    }

    bool operator==(const Bag& otherBag) const
    {
        return number == otherBag.number;
    }

    std::string toString() const
    {
        std::string ret = std::string("") + static_cast<char>(type) + std::string("-Bag ");
        if (parentNumber.has_value())
        {
            ret += std::to_string(number) + std::string(" with parent: ")
            + std::to_string(parentNumber.value()) + std::string(" and vertices: ");
        }
        else
        {
            ret += std::string("(Root) ") + std::to_string(number);
            return ret;
        }
        for (auto i = 0; i < bagElements.size(); ++i)
        {
            ret += std::to_string(bagElements[i]);
            if (i + 1 < bagElements.size())
            {
                ret += std::string(", ");
            }
        }
        if (introduceEdges.size() > 0)
        {
            ret += "; Introduced edges: [";
            for (auto i = 0; i < introduceEdges.size(); ++i)
            {
                ret += std::string("(") + std::to_string(introduceEdges[i].first) + std::string(",") + std::to_string(introduceEdges[i].second) + std::string(")");
                if (i + 1 < introduceEdges.size())
                {
                    ret += std::string(", ");
                }
            }
            ret += "]";
        }
        return ret;
    }

    std::string toStringState() const
    {
        std::string ret = std::string("");
        if (c.size() > 0)
        {
            auto index = 0;
            for (const auto& [coloring, value] : c)
            {
                ret += std::string("Coloring NR.") + std::to_string(index) + std::string("(c=") + std::to_string(value) + std::string("):\n");
                for (const auto ptr : coloring)
                {
                    const auto& [vertex, color] = *ptr.get();
                    ret += std::string("\tNode ") + std::to_string(vertex) + std::string(" -> ") + std::to_string(static_cast<uint8_t>(color)) + std::string("\n");
                }
                index++;
            }
        }

        return ret;
    }
};
/************************************************************************************************************************/

/* Declaring Functions used for the different Bag-Types during traversal */
void introduceVertexNode(Bag * const, const Bag * const);
void joinNode(Bag * const, const Bag * const, const Bag * const);
void forgetNode(Bag * const, const Bag * const);
/**
 * Additionally to the common bag-types 'introduceEdge' is necessary as a way to 
 * introduce an edge between two vertices in the original graph.
 * This function is used to update the current bag-state for when we get the information, 
 * that a vertex of the bag might not longer be required to be dominated.
 * (By switching the value of the White/Black or Black/White colorings with the value of a
 *  grey coloring instead of the white one) 
 */
void introduceEdge(Bag * const, const Bag * const, const std::pair<int, int>&);

/**
 * Main function reads a .gr file by calling a python script that uses sage to handle
 * the construction of a nice-tree-decomposition, then starts min-dominating-set calculation
 */
int main(int argc, char** argv)
{
    // this block calls the python script 
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_gr_file> {<path_to_td_file>}\n";
        return 1;
    }
    const auto tempFilename = "_decomp_.temp";
    std::ofstream out{tempFilename};
    if (!out) {
        std::cerr << "Could not create " << tempFilename << '\n';
        return 1;
    }
    out.close();
    std::string command = "python3 read.py ";
    command += argv[1];
    command += " ";
    if (argc == 3)
    {
        command += argv[2];
        command += " ";
    }
    command += tempFilename;
    int ret = std::system(command.c_str());

    // the python script writes the nice TD into a file which is now parsed into the Bag datastructure
    std::ifstream in(tempFilename);
    if (!in)
    {
        std::cerr << "Could not open " << tempFilename << std::endl;
        return 1;
    }
    std::vector<std::unique_ptr<Bag>> bags;
    auto root = std::make_unique<Bag>(
        0, BagType::Forget, std::nullopt, std::vector<int>{}, std::vector<std::pair<int, int>>{}
    );
    bags.push_back(std::move(root));
    std::string line;
    std::regex pattern1(R"(\((\d+)\,\{([^}]*)\})"); // matches the bag number and everything inside curly brackets accept closed curly bracket
    std::regex pattern2(R"(\[\((\d+))"); // matches the bag number of the parent bag
    std::regex pattern3(R"(\((\d+)\,(\d+)\))"); // matches an edge
    std::smatch match;
    std::getline(in, line); // first line is root node, which we already have
    while (std::getline(in, line))
    {
        std::istringstream strstr(line);
        std::string word;

        // first word in line is the bag number and list of vertices inside the bag
        strstr >> word;
        std::uint16_t currentBagNumber;
        std::vector<int> currentVertices;
        if (std::regex_search(word, match, pattern1))
        {
            currentBagNumber = std::stoi(match[1].str());
            std::stringstream strstrTemp(match[2].str());
            std::string currentVertex;
            while (std::getline(strstrTemp, currentVertex, ','))
            {
                currentVertices.push_back(std::stoi(currentVertex));
            }
        }
        else
        {
            std::cerr << "Error while trying to match word: " << word << std::endl;
        }
        
        // second word in line is type of bag
        strstr >> word;
        BagType currentBagType = static_cast<BagType>(word[0]);

        // third word in line contains parent node
        strstr >> word;
        std::uint16_t currentParentBag;
        if (std::regex_search(word, match, pattern2))
        {
            currentParentBag = std::stoi(match[1].str());
        }
        else
        {
            std::cerr << "Error while trying to match word: " << word << std::endl;
        }

        // fourth word in line contains possible introduce edges 
        std::vector<std::pair<int, int>> introduceEdges;
        strstr >> word;
        if (word.size() > 2)
        {
            // strip brackets
            word = word.substr(1, word.size() - 2);
            auto wordIt = std::cbegin(word); 
            while (std::regex_search(wordIt, std::cend(word), match, pattern3))
            {
                introduceEdges.emplace_back(std::stoi(match[1].str()), std::stoi(match[2].str()));
                wordIt = match.suffix().first;
            }
        }

        // but the bag in the bags
        auto bag = std::make_unique<Bag>(
            currentBagNumber, currentBagType,
            std::make_optional(currentParentBag),
            std::move(currentVertices),
            std::move(introduceEdges)
        );
        bags.push_back(std::move(bag));
    }
    if (!in.eof())
    {
        std::cerr << "Error while reading " << tempFilename << std::endl;
        return 1;
    }
    in.close();
    // remove the temp file which we read python output from
    if (!std::filesystem::remove(tempFilename)) {
        std::cerr << "File: " << tempFilename << "not found or couldn't be deleted" << std::endl;
    }


    /**************************/
    /* LOGIC-PART begins here */
    /**************************/
    // set children in all bags, then compute postorder traversal
    for (const auto &bag : bags)
    {
        if (bag.get()->parentNumber.has_value())
        {
            const auto parentNumber = bag.get()->parentNumber.value();
            if (!bags[parentNumber].get()->child1.has_value())
            {
                bags[parentNumber].get()->child1 = std::make_optional(bag.get()->number);
            }
            else
            {
                bags[parentNumber].get()->child2 = std::make_optional(bag.get()->number);
            }
        }
    }
    std::vector<std::uint16_t> postorder;
    postorder.reserve(bags.size());
    std::function<void(uint16_t)> postorderTraversal = [&postorder, &bags, &postorderTraversal](uint16_t number) -> void
    {
        const auto bag = bags[number].get();

        if (bag->child1.has_value())
        {
            postorderTraversal(bag->child1.value());
        }
        if (bag->child2.has_value())
        {
            postorderTraversal(bag->child2.value());
        }

        postorder.push_back(bag->number);
    };
    postorderTraversal(0);

    // iterate bags in correct order and call appropriate bag-logic functions 
    for (auto &&number : postorder)
    {
        const auto bag = bags[number].get();

        if (!bag->parentNumber.has_value())
        {
            // found root, finished postorder-traversal now print the min cost of the only child
            const auto child = bags[bag->child1.value()].get();
            assert(child->bagElements.size() == 1); // property of nice TD
            auto minDominatingSetSize = std::numeric_limits<int>::max();
            for (const auto &pair : child->c)
            {
                if (minDominatingSetSize > pair.second)
                {
                    minDominatingSetSize = pair.second;
                }
            }
            std::cout << "The size of the minimum-dominating-set in this graph is: " << minDominatingSetSize << "." << std::endl;
        }
        else if (bag->type == BagType::Intro)
        {
            introduceVertexNode(bag, bags[bag->child1.value()].get());
        }
        else if (bag->type == BagType::Forget)
        {
            forgetNode(bag, bags[bag->child1.value()].get());
        }
        else if (bag->type == BagType::Join)
        {
            joinNode(bag, bags[bag->child1.value()].get(), bags[bag->child2.value()].get());
        }

        // introduce edges
        for (const auto &edge : bag->introduceEdges)
        {
            introduceEdge(bag, bags[bag->child1.value()].get(), edge);
        }
    }

    /* prints all parsed bags with a lot more info
    for (auto &&i : bags)
    {
        std::cout << i.get()->toString() << std::endl;
        std::cout << "Child1: " << i.get()->child1.value_or(-1) << ", Child2: " << i.get()->child2.value_or(-1) << std::endl;
        std::cout << i.get()->toStringState() << std::endl;
    }*/
    return 0;
}

void introduceEdge(Bag * const bag, const Bag * const child, const std::pair<int, int>& uv)
{
    const auto uBlack = lookup(uv.first, Color::Black);
    const auto vWhite = lookup(uv.second, Color::White);
    const auto uWhite = lookup(uv.first, Color::White);
    const auto vBlack = lookup(uv.second, Color::Black);

    for (auto &[coloring, value] : bag->c)
    {
        if (std::find(coloring.cbegin(), coloring.cend(), uBlack) != coloring.end() &&
            std::find(coloring.cbegin(), coloring.cend(), vWhite) != coloring.end())
        {
            Coloring vGreyCopy;
            vGreyCopy.reserve(coloring.size());
            for (const auto ptr : coloring)
            {
                if (ptr == vWhite)
                {
                    vGreyCopy.push_back(lookup(uv.second, Color::Grey));
                }
                else
                {
                    vGreyCopy.push_back(ptr);
                }
            }
            value = bag->c.at(vGreyCopy);
        }
        else if (
            std::find(coloring.cbegin(), coloring.cend(), uWhite) != coloring.end() &&
            std::find(coloring.cbegin(), coloring.cend(), vBlack) != coloring.end())
        {
            Coloring uGreyCopy;
            uGreyCopy.reserve(coloring.size());
            for (const auto ptr : coloring)
            {
                if (ptr == uWhite)
                {
                    uGreyCopy.push_back(lookup(uv.first, Color::Grey));
                }
                else
                {
                    uGreyCopy.push_back(ptr);
                }
            }
            value = bag->c.at(uGreyCopy); 
        }
    }
}

void joinNode(Bag * const bag, const Bag * const child1, const Bag * const child2)
{
    assert(bag->consistentColorings.size() > 0);
    
    for (const auto &consistentTriple : bag->consistentColorings)
    {
        const auto &coloring = std::get<0>(consistentTriple); 
        auto compatibleSetSize = 0;
        for (const auto ptr : coloring)
        {
            if ((*ptr.get()).second == Color::Black)
            {
                compatibleSetSize++;
            }
        }
        auto consistentColoring1Value = child1->c.at(std::get<1>(consistentTriple));
        auto consistentColoring2Value = child2->c.at(std::get<2>(consistentTriple));
        
        // we need to compute 'c1+c2-compatibleSize', we treat max() as infinity so overflow checks work a bit differently
        auto res = 0;
        if (consistentColoring1Value == std::numeric_limits<int>::max() ||
            consistentColoring2Value == std::numeric_limits<int>::max())
        {
            res = std::numeric_limits<int>::max();
        }
        else
        {
            res = consistentColoring1Value + consistentColoring2Value - compatibleSetSize;
        }

        // adopt min of all consistent colorings
        bag->c.at(coloring) = std::min(bag->c.at(coloring), res);
    }
}

void forgetNode(Bag * const bag, const Bag * const child)
{
    const auto w = std::accumulate(child->bagElements.cbegin(), child->bagElements.cend(), 0) -
        std::accumulate(bag->bagElements.cbegin(), bag->bagElements.cend(), 0);

    for (auto &[coloring, value] : bag->c)
    {
        auto blackCopy = coloring;
        blackCopy.push_back(lookup(w, Color::Black));
        auto whiteCopy = coloring;
        whiteCopy.push_back(lookup(w, Color::White));

        value = std::min(child->c.at(blackCopy), child->c.at(whiteCopy));
    }
}

void introduceVertexNode(Bag * const bag, const Bag * const child)
{
    const auto v = std::accumulate(bag->bagElements.cbegin(), bag->bagElements.cend(), 0) -
        std::accumulate(child->bagElements.cbegin(), child->bagElements.cend(), 0);

    for (auto &[coloring, value] : bag->c)
    {
        if (child->type == BagType::Leaf)
        {
            assert(coloring.size() == 1);
            const auto& pair = *(coloring.front().get());
            if (pair.second == Color::White)
            {
                value = std::numeric_limits<int>::max();
            }
            else if (pair.second == Color::Grey)
            {
                value = 0;
            }
            else
            {
                assert(pair.second == Color::Black);
                value = 1;
            }
            continue;
        }

        if (std::find(coloring.cbegin(), coloring.cend(), lookup(v, Color::White)) != coloring.end())
        {
            value = std::numeric_limits<int>::max();
            continue;
        }
        else
        {
            Coloring copyWithoutNewVertex;
            copyWithoutNewVertex.reserve(coloring.size() - 1);
            Color newVertexColor;

            for (const auto ptr : coloring)
            {
                const auto& [vertex, color] = *ptr.get();
                if (vertex != v)
                {
                    copyWithoutNewVertex.push_back(ptr);
                }
                else
                {
                    newVertexColor = color;
                }
            }

            auto childValue = child->c.at(copyWithoutNewVertex); 
            if (newVertexColor == Color::Grey || childValue == std::numeric_limits<int>::max())
            {
                value = childValue;
            }
            else
            {
                value = 1 + childValue;
            }
        }
    }
}
