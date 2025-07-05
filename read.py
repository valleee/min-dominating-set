import sys
from pathlib import Path
from sage.all import *
from sage.graphs.graph_decompositions.tree_decomposition import make_nice_tree_decomposition, label_nice_tree_decomposition, is_valid_tree_decomposition, width_of_tree_decomposition

def gr_to_graph_library(path):
    firstLine = False
    m = None
    G = {}

    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line[0] == "c":
                continue
            if line.startswith("p"):
                if firstLine:
                    raise ValueError("This is not a valid .gr file: more than 1 line starting with 'p'")
                firstLine = True
                _, format, n, m = line.split()
                if format != "tw":
                    raise ValueError("This is not a valid .gr file")
                n = int(n)
                m = int(m)
                for v in range(1, n+1):
                    G[v] = []
            else:
                if not firstLine:
                    raise ValueError("This is not a valid .gr file: first non-comment line must start with 'p'")
                u, v = line.split()
                u = int(u)
                v = int(v)
                if u not in G or v not in G or v in G[u]:
                    raise ValueError(".gr file is wrongly formatted")
                G[u].append(v)
                m = m - 1

    if m != 0:
        raise ValueError("This is not a valid .gr file: wrong amount of edges")

    return G

def read_td(path):
    firstLine = False
    bags = {}
    bag_edges = []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            
            if not line or line.startswith("c"):
                continue
            
            if line.startswith("s"):
                if firstLine:
                    raise ValueError("This is not a valid .td file: more than 1 line starting with 's'")
                firstLine = true
                _, td, _, _, _ = line.split()
                if td != "td":
                    raise ValueError("This is not a valid .td file: no 'td' word")
                
            elif line.startswith("b"):
                if not firstLine:
                    raise ValueError("This is not a valid .td file: the first non-comment line must start with 's'")
                data = line.split()
                bag_id = int(data[1])
                vertices = [Integer(v) for v in list(data[2:])]
                bags[bag_id] = Set(vertices)

            else:
                u, v = map(int, line.split())
                bag_edges.append((bags[u],bags[v]))

    TD = Graph(bag_edges)
    return TD


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise Exception("add path to .gr/.td file")
    input_gr = Path(sys.argv[1])
    input_td = None
    outputFilePath = None
    G = None
    TD_G = None
    if len(sys.argv) > 2:
        if len(sys.argv) > 3:
            input_td = Path(sys.argv[2])
            outputFilePath = Path(sys.argv[3])
        elif Path(sys.argv[2]).suffix == ".td":
            input_td = Path(sys.argv[2])
        else:
            outputFilePath = Path(sys.argv[2])

    if input_gr.suffix == ".gr":
        G = Graph(gr_to_graph_library(input_gr))
        if input_td is None:
            print("computing optimal TD (NP-hard) using sage...")
            # computing an optimal TD is an NP-complete problem and thus the complexity of the next line dwarfes every
            # other operation in this script
            TD_G = G.treewidth(certificate=True)
            print("...finished computing optimal TD, width: ", width_of_tree_decomposition(G, TD_G))
        else:
            TD_G = read_td(input_td)
            if not is_valid_tree_decomposition(G, TD_G):
                raise ValueError("The parsed .td file does not form a valid tree-decomposition for graph parsed from the .gr file")
    else:
        raise Exception("Invalid file. Input must be read from .gr file")

    nice_TD_G = make_nice_tree_decomposition(G, TD_G)
    root = sorted(nice_TD_G.vertices())[Integer(0)]
    labelled_nice_TD_G = label_nice_tree_decomposition(nice_TD_G, root, directed=True)
    sorted_labelled_nice_TD_G = sorted(labelled_nice_TD_G)

    edge_insert_count = 0
    highestVerts = set()
    introduceEdgeNodes = {}
    for node in sorted_labelled_nice_TD_G:
        introduceEdgeNodes[node] = []
        for v in node[1]:
            if v not in highestVerts:
                for u in highestVerts:
                    if G.has_edge((u,v)):
                        edge_insert_count += 1
                        introduceEdgeNodes[node].append((u,v))
                highestVerts.add(v)

    if edge_insert_count != G.num_edges():
        raise ValueError("wrong amount of edges are introduced")
    
    if outputFilePath is None:
        for node in sorted_labelled_nice_TD_G:
            print("".join(str(node).split()), "".join(str(labelled_nice_TD_G.get_vertex(node)).split()), "".join(str(labelled_nice_TD_G.neighbors(node)).split()))
    else:
        with open(outputFilePath, "w", encoding="utf-8") as f:
            for node in sorted_labelled_nice_TD_G:
                introduceEdgeNodesString = ""
                if len(introduceEdgeNodes[node]) > 0:
                    introduceEdgeNodesString = "".join(str(introduceEdgeNodes[node]).split())
                else:
                    introduceEdgeNodesString = []

                print("".join(str(node).split()),
                      "".join(str(labelled_nice_TD_G.get_vertex(node)).split()),
                      "".join(str(labelled_nice_TD_G.neighbors(node)).split()),
                      introduceEdgeNodesString,
                      file=f)
