import numpy as np

def format_time_data_for_plot(data: dict[int, list[float]]):
    labels = sorted(data.keys())
    box_data = [data[k] for k in labels]
    mean_data = [np.mean(data[k]) for k in labels]

    return {
        "labels": labels,
        "box_series": {"Temps par taille": box_data},
        "line_series": {"Temps moyen": mean_data}
    }
    
def format_complexity_data_for_plot(data: dict[int, (list[float], list[int])]):
    labels = sorted(data.keys())
    box_time = []
    box_backjumps = []
    mean_time = []
    mean_backjumps = []
    for k in labels:
        times, backjumps = data[k]
        box_time.append(times)
        box_backjumps.append(backjumps)
        mean_time.append(np.mean(times))
        mean_backjumps.append(np.mean(backjumps))

    return {
        "labels": labels,
        "box_series_times": {"Temps par taille": box_time},
        "line_series_times": {"Temps moyen": mean_time},
        "box_series_backjumps": {"Backjumps par taille": box_backjumps},
        "line_series_bakjumps": {"Backjumps moyen": mean_backjumps}
    }

