from analysis.randomModifiedVar_analysis import flip_variable_in_dimacs

def run_randomModifiedVar_analysis(config: dict, pairs_xy: dict[int, tuple[int, int]], paths: dict):
    
    flip_variable_in_dimacs
    """ data = time_evaluation(pairs=pairs_xy, dimacs_dir=paths["DIMACS_DIR"], simplifie=config["simplifie"], bloat=config["bloat"])
    
    plot_data = format_time_data_for_plot(data)
    
    plot_figure(
        plots=[
            {
                "type": "line",
                "title": "Temps moyen",
                "data": {
                    "labels": plot_data["labels"],
                    "series": plot_data["line_series"]
                },
                "xlabel": "Taille opérande [bits]",
                "ylabel": "Temps [s]"
            },
            {
                "type": "box",
                "title": "Distribution des temps",
                "data": {
                    "labels": plot_data["labels"],
                    "series": plot_data["box_series"]
                },
                "xlabel": "Taille opérande [bits]",
                "ylabel": "Temps [s]"
            }
        ],
        save_path=f"{paths["GRAPH_DIR"]}/time_analysis.png",
        layout=(1, 2),
        title="Analyse des temps"
    )
 """