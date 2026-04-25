from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

import numpy as np

from ecodex_ml import (
    build_feature_matrix,
    collect_samples,
    predict_scores,
    save_model,
    split_samples,
    standardize_features,
    top_predictions,
)


DEFAULT_DATASET = Path(r"C:\Users\Kaitlyn June\Downloads\Ecodex\dataset")
DEFAULT_MODEL = Path(__file__).resolve().parent / "model" / "ecodex_baseline.npz"


def evaluate_split(
    name: str,
    features: np.ndarray,
    labels: np.ndarray,
    train_features: np.ndarray,
    train_labels: np.ndarray,
    class_names: list[str],
    k: int,
) -> dict:
    correct = 0
    records: list[dict] = []
    per_class_total = Counter()
    per_class_correct = Counter()

    for feature, label_index in zip(features, labels, strict=True):
        scores = predict_scores(feature, train_features, train_labels, class_names, k=k)
        predicted = int(np.argmax(scores))
        confidence = float(scores[predicted])

        actual_label = class_names[int(label_index)]
        predicted_label = class_names[predicted]

        per_class_total[actual_label] += 1
        if predicted == int(label_index):
            correct += 1
            per_class_correct[actual_label] += 1

        records.append(
            {
                "actual": actual_label,
                "predicted": predicted_label,
                "confidence": round(confidence, 4),
                "top3": top_predictions(scores, class_names, limit=3),
            }
        )

    accuracy = (correct / len(labels)) if len(labels) else 0.0
    per_class_accuracy = {
        class_name: round(per_class_correct[class_name] / total, 4)
        for class_name, total in sorted(per_class_total.items())
    }

    return {
        "name": name,
        "count": int(len(labels)),
        "accuracy": round(accuracy, 4),
        "per_class_accuracy": per_class_accuracy,
        "examples": records[:12],
    }


def choose_threshold(
    validation_features: np.ndarray,
    validation_labels: np.ndarray,
    train_features: np.ndarray,
    train_labels: np.ndarray,
    class_names: list[str],
    k: int,
) -> float:
    if len(validation_labels) == 0:
        return 0.55

    confidences_correct: list[float] = []
    confidences_wrong: list[float] = []

    for feature, label_index in zip(validation_features, validation_labels, strict=True):
        scores = predict_scores(feature, train_features, train_labels, class_names, k=k)
        predicted = int(np.argmax(scores))
        confidence = float(scores[predicted])
        if predicted == int(label_index):
            confidences_correct.append(confidence)
        else:
            confidences_wrong.append(confidence)

    if not confidences_correct or not confidences_wrong:
        return 0.55

    threshold = (float(np.mean(confidences_correct)) + float(np.mean(confidences_wrong))) / 2.0
    return round(float(np.clip(threshold, 0.45, 0.8)), 2)


def main() -> None:
    parser = argparse.ArgumentParser(description="Train a lightweight offline EcoDex baseline model.")
    parser.add_argument("--dataset", type=Path, default=DEFAULT_DATASET, help="Folder containing one subfolder per class.")
    parser.add_argument("--out", type=Path, default=DEFAULT_MODEL, help="Where to save the trained .npz model.")
    parser.add_argument("--seed", type=int, default=42, help="Random seed used for the split.")
    parser.add_argument("--k", type=int, default=7, help="Number of neighbors used by the classifier.")
    args = parser.parse_args()

    labels, samples = collect_samples(args.dataset)
    train_samples, val_samples, test_samples = split_samples(samples, seed=args.seed)

    train_features_raw, train_labels = build_feature_matrix(train_samples, augment_flips=True)
    val_features_raw, val_labels = build_feature_matrix(val_samples)
    test_features_raw, test_labels = build_feature_matrix(test_samples)

    train_features, feature_mean, feature_std = standardize_features(train_features_raw, train_features_raw)
    val_features, _, _ = standardize_features(train_features_raw, val_features_raw, feature_mean, feature_std)
    test_features, _, _ = standardize_features(train_features_raw, test_features_raw, feature_mean, feature_std)

    threshold = choose_threshold(
        validation_features=val_features,
        validation_labels=val_labels,
        train_features=train_features,
        train_labels=train_labels,
        class_names=labels,
        k=args.k,
    )

    metrics = {
        "dataset": str(args.dataset),
        "labels": labels,
        "split_counts": {
            "train": len(train_samples),
            "validation": len(val_samples),
            "test": len(test_samples),
        },
        "image_count_by_class": {
            label: sum(1 for sample in samples if sample.class_name == label)
            for label in labels
        },
        "k": args.k,
        "suggested_threshold": threshold,
        "validation": evaluate_split(
            "validation",
            val_features,
            val_labels,
            train_features,
            train_labels,
            labels,
            args.k,
        ),
        "test": evaluate_split(
            "test",
            test_features,
            test_labels,
            train_features,
            train_labels,
            labels,
            args.k,
        ),
    }

    save_model(
        out_path=args.out,
        labels=labels,
        train_features=train_features,
        train_labels=train_labels,
        feature_mean=feature_mean,
        feature_std=feature_std,
        k=args.k,
        suggested_threshold=threshold,
        metrics=metrics,
    )

    print("EcoDex baseline model trained.")
    print(f"Dataset: {args.dataset}")
    print(f"Saved model: {args.out}")
    print(f"Classes: {', '.join(labels)}")
    print(f"Split counts: {json.dumps(metrics['split_counts'])}")
    print(f"Validation accuracy: {metrics['validation']['accuracy']:.2%}")
    print(f"Test accuracy: {metrics['test']['accuracy']:.2%}")
    print(f"Suggested confidence threshold: {threshold:.2f}")


if __name__ == "__main__":
    main()
