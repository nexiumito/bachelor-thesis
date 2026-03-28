from analysis.complexity_analysis import complexity_evaluation
from plots.formater import format_complexity_data_for_plot
from plots.plots import plot_figure

def run_complexity_analysis(config: dict, pairs_xy: dict[int, tuple[int, int]], paths: dict):
    
    data = complexity_evaluation(pairs=pairs_xy, dimacs_dir=paths["DIMACS_DIR"], simplifie=config["simplifie"], bloat=config["bloat"])
    
    plot_data = format_complexity_data_for_plot(data)
    
    print(plot_data)
    
    plot_figure(
        plots=[
            {
                "type": "line",
                "title": "Temps moyen",
                "data": {
                    "labels": plot_data["labels"],
                    "series": plot_data["line_series_times"]
                },
                "xlabel": "Taille opérande [bits]",
                "ylabel": "Temps [s]"
            },
            {
                "type": "box",
                "title": "Distribution des temps",
                "data": {
                    "labels": plot_data["labels"],
                    "series": plot_data["box_series_times"]
                },
                "xlabel": "Taille opérande [bits]",
                "ylabel": "Temps [s]"
            },
            {
                "type": "line",
                "title": "Backjumps moyen",
                "data": {
                    "labels": plot_data["labels"],
                    "series": plot_data["line_series_bakjumps"]
                },
                "xlabel": "Taille opérande [bits]",
                "ylabel": "Nombre de retours arrière (backjumps)"
            },
            {
                "type": "box",
                "title": "Distribution des backjumps",
                "data": {
                    "labels": plot_data["labels"],
                    "series": plot_data["box_series_backjumps"]
                },
                "xlabel": "Taille opérande [bits]",
                "ylabel": "Nombre de retours arrière (backjumps)"
            }
        ],
        save_path=f"{paths["GRAPH_DIR"]}/complexity_analysis.png",
        layout=(2, 2),
        title="Analyse des temps et des backjumps"
    )
