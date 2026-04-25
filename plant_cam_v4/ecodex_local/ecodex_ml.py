from __future__ import annotations

import io
import json
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import numpy as np
from PIL import Image, ImageOps, UnidentifiedImageError


IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
FEATURE_SIZE = (20, 20)


@dataclass
class Sample:
    path: Path
    class_name: str
    class_index: int


def list_class_dirs(dataset_root: Path) -> list[Path]:
    return sorted(
        [path for path in dataset_root.iterdir() if path.is_dir()],
        key=lambda path: path.name.lower(),
    )


def collect_samples(dataset_root: Path) -> tuple[list[str], list[Sample]]:
    class_dirs = list_class_dirs(dataset_root)
    if not class_dirs:
        raise ValueError(f"No class folders were found in {dataset_root}")

    labels = [path.name for path in class_dirs]
    samples: list[Sample] = []
    for class_index, class_dir in enumerate(class_dirs):
        for image_path in sorted(class_dir.iterdir()):
            if image_path.is_file() and image_path.suffix.lower() in IMAGE_EXTENSIONS:
                samples.append(
                    Sample(
                        path=image_path,
                        class_name=class_dir.name,
                        class_index=class_index,
                    )
                )

    if not samples:
        raise ValueError(f"No images were found in {dataset_root}")
    return labels, samples


def split_samples(
    samples: Iterable[Sample],
    seed: int = 42,
    val_ratio: float = 0.15,
    test_ratio: float = 0.15,
) -> tuple[list[Sample], list[Sample], list[Sample]]:
    grouped: dict[str, list[Sample]] = {}
    for sample in samples:
        grouped.setdefault(sample.class_name, []).append(sample)

    rng = random.Random(seed)
    train_samples: list[Sample] = []
    val_samples: list[Sample] = []
    test_samples: list[Sample] = []

    for class_name, class_samples in grouped.items():
        rng.shuffle(class_samples)
        total = len(class_samples)
        if total < 3:
            raise ValueError(
                f"Class '{class_name}' only has {total} image(s). Add at least 3 to train safely."
            )

        val_count = max(1, int(round(total * val_ratio)))
        test_count = max(1, int(round(total * test_ratio)))
        train_count = total - val_count - test_count

        while train_count < 2 and (val_count > 1 or test_count > 1):
            if val_count >= test_count and val_count > 1:
                val_count -= 1
            elif test_count > 1:
                test_count -= 1
            train_count = total - val_count - test_count

        if train_count < 2:
            raise ValueError(
                f"Class '{class_name}' does not have enough images after the split. "
                f"Add more images before training."
            )

        train_samples.extend(class_samples[:train_count])
        val_samples.extend(class_samples[train_count : train_count + val_count])
        test_samples.extend(class_samples[train_count + val_count :])

    return train_samples, val_samples, test_samples


def _fit_image(image: Image.Image) -> Image.Image:
    image = ImageOps.exif_transpose(image).convert("RGB")
    return ImageOps.fit(image, FEATURE_SIZE, method=Image.Resampling.BILINEAR)


def extract_features(image: Image.Image) -> np.ndarray:
    image = _fit_image(image)
    rgb = np.asarray(image, dtype=np.float32) / 255.0
    gray = np.asarray(image.convert("L"), dtype=np.float32) / 255.0
    hsv = np.asarray(image.convert("HSV"), dtype=np.float32) / 255.0

    rgb_flat = rgb.reshape(-1)

    h_hist, _ = np.histogram(hsv[..., 0], bins=12, range=(0.0, 1.0))
    s_hist, _ = np.histogram(hsv[..., 1], bins=8, range=(0.0, 1.0))
    v_hist, _ = np.histogram(hsv[..., 2], bins=8, range=(0.0, 1.0))

    gy, gx = np.gradient(gray)
    mag = np.sqrt((gx * gx) + (gy * gy))
    edge_hist, _ = np.histogram(mag, bins=8, range=(0.0, 1.0))

    color_stats = np.concatenate(
        [
            rgb.mean(axis=(0, 1)),
            rgb.std(axis=(0, 1)),
            np.asarray([gray.mean()], dtype=np.float32),
            np.asarray([gray.std()], dtype=np.float32),
        ]
    )

    feature = np.concatenate(
        [
            rgb_flat,
            normalize_histogram(h_hist),
            normalize_histogram(s_hist),
            normalize_histogram(v_hist),
            normalize_histogram(edge_hist),
            color_stats,
        ]
    )
    return feature.astype(np.float32)


def extract_features_from_path(path: Path) -> np.ndarray:
    try:
        with Image.open(path) as image:
            return extract_features(image)
    except UnidentifiedImageError as exc:
        raise ValueError(f"Could not read image: {path}") from exc


def extract_features_from_bytes(image_bytes: bytes) -> np.ndarray:
    try:
        with Image.open(io.BytesIO(image_bytes)) as image:
            return extract_features(image)
    except UnidentifiedImageError as exc:
        raise ValueError("Could not decode image bytes") from exc


def normalize_histogram(values: np.ndarray) -> np.ndarray:
    values = values.astype(np.float32)
    total = float(values.sum())
    if total <= 0:
        return values
    return values / total


def build_feature_matrix(samples: list[Sample], augment_flips: bool = False) -> tuple[np.ndarray, np.ndarray]:
    features: list[np.ndarray] = []
    labels: list[int] = []

    for sample in samples:
        base = extract_features_from_path(sample.path)
        features.append(base)
        labels.append(sample.class_index)

        if augment_flips:
            with Image.open(sample.path) as image:
                flipped = ImageOps.mirror(ImageOps.exif_transpose(image).convert("RGB"))
                features.append(extract_features(flipped))
                labels.append(sample.class_index)

    return np.vstack(features), np.asarray(labels, dtype=np.int32)


def standardize_features(
    train_features: np.ndarray,
    features: np.ndarray,
    feature_mean: np.ndarray | None = None,
    feature_std: np.ndarray | None = None,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    if feature_mean is None:
        feature_mean = train_features.mean(axis=0)
    if feature_std is None:
        feature_std = train_features.std(axis=0)
    feature_std = np.where(feature_std < 1e-6, 1.0, feature_std)
    standardized = (features - feature_mean) / feature_std
    return standardized.astype(np.float32), feature_mean.astype(np.float32), feature_std.astype(np.float32)


def predict_scores(
    feature_vector: np.ndarray,
    train_features: np.ndarray,
    train_labels: np.ndarray,
    class_names: list[str],
    k: int = 7,
) -> np.ndarray:
    if len(train_features) == 0:
        raise ValueError("Model has no training features")

    k = max(1, min(k, len(train_features)))
    distances = np.linalg.norm(train_features - feature_vector, axis=1)
    nearest = np.argpartition(distances, k - 1)[:k]

    weights = 1.0 / np.maximum(distances[nearest], 1e-6)
    scores = np.zeros(len(class_names), dtype=np.float32)
    for weight, label_index in zip(weights, train_labels[nearest], strict=True):
        scores[int(label_index)] += float(weight)

    total = float(scores.sum())
    if total > 0:
        scores /= total
    return scores


def top_predictions(scores: np.ndarray, class_names: list[str], limit: int = 3) -> list[dict[str, float | str]]:
    ordered = np.argsort(scores)[::-1][:limit]
    return [
        {"label": class_names[int(index)], "confidence": round(float(scores[int(index)]), 4)}
        for index in ordered
    ]


def save_model(
    out_path: Path,
    labels: list[str],
    train_features: np.ndarray,
    train_labels: np.ndarray,
    feature_mean: np.ndarray,
    feature_std: np.ndarray,
    k: int,
    suggested_threshold: float,
    metrics: dict,
) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        out_path,
        labels=np.asarray(labels),
        train_features=train_features.astype(np.float32),
        train_labels=train_labels.astype(np.int32),
        feature_mean=feature_mean.astype(np.float32),
        feature_std=feature_std.astype(np.float32),
        k=np.asarray([k], dtype=np.int32),
        suggested_threshold=np.asarray([suggested_threshold], dtype=np.float32),
    )
    metrics_path = out_path.with_suffix(".metrics.json")
    metrics_path.write_text(json.dumps(metrics, indent=2), encoding="utf-8")


def load_model(model_path: Path) -> dict:
    with np.load(model_path, allow_pickle=False) as data:
        return {
            "labels": data["labels"].tolist(),
            "train_features": data["train_features"].astype(np.float32),
            "train_labels": data["train_labels"].astype(np.int32),
            "feature_mean": data["feature_mean"].astype(np.float32),
            "feature_std": data["feature_std"].astype(np.float32),
            "k": int(data["k"][0]),
            "suggested_threshold": float(data["suggested_threshold"][0]),
        }


def standardize_single(feature: np.ndarray, feature_mean: np.ndarray, feature_std: np.ndarray) -> np.ndarray:
    feature_std = np.where(feature_std < 1e-6, 1.0, feature_std)
    return ((feature - feature_mean) / feature_std).astype(np.float32)
