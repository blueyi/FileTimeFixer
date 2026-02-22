# Similar Image Detection Design

This document describes how to add **similar image detection** to the FileTimeFixer project: find visually similar images within or across directories, with **configurable similarity levels**.

---

## 1. Goals and use cases

- **Goal**: Identify “visually similar” images in large photo sets (near-duplicates, same scene different framing, same image at different size/compression).
- **Levels**: User chooses a similarity level (e.g. strict / medium / loose); higher level means looser “similar” criteria.
- **Relation to main project**: Standalone capability; can be used before or after time-fix (e.g. find similar → decide keep/delete → run time fix).

---

## 2. Technology overview

| Approach | Typical stack | Pros | Cons | Best level |
|----------|----------------|------|------|------------|
| **Perceptual hash (pHash/dHash/aHash)** | imagehash (Python), libpHash (C++) | Simple, fast, no GPU | Sensitive to crop/rotation | Strict–medium |
| **Histogram** | OpenCV (Python/C++) | Simple, color similarity | Insensitive to composition | Auxiliary |
| **Local features (SIFT/ORB)** | OpenCV | Scale/rotation/occlusion robust | Heavier, tuning | Medium–loose |
| **Deep features (CNN embeddings)** | ResNet/ViT + vector similarity | Best semantic similarity | Model/runtime, size | Loose, semantic |

**Suggested mapping by level**:

- **Level 1 (strict)**: **Perceptual hash** with tight threshold → “almost same image” (compression, slight resize, light edits).
- **Level 2 (medium)**: **Perceptual hash** with looser threshold + optional **histogram/features** → same image, different size/crop/edits.
- **Level 3 (loose)**: **Local feature matching** (e.g. ORB) or **CNN embedding + cosine similarity** → same scene, different framing.

---

## 3. Similarity level definition (configurable)

Map “similarity level” to **algorithm + threshold**:

| Level | Name (example) | Strategy | Typical threshold |
|-------|----------------|----------|-------------------|
| **1** | Strict | pHash or dHash only | Hamming distance ≤ 5 (configurable 0–10) |
| **2** | Medium | pHash + optional aHash/histogram | Hamming ≤ 15; histogram correlation > 0.85 |
| **3** | Loose | Feature matching or small embedding model | Match count ≥ 20 or embedding cosine > 0.75 |

Level can be set via CLI or config, e.g. `--similarity-level 1|2|3`, optional `--similarity-threshold N` to override Hamming for hash-based levels.

---

## 4. Implementation path

### 4.1 Python (recommended first)

- **Location**: Separate package under repo root (e.g. `similar_images/`) with its own CLI; or a submodule under `python/filetimefixer/`.
- **Dependencies**: `imagehash`, `Pillow` (required); optional `opencv-python`, `numpy`; optional `torch`+`timm` for level 3.
- **Interface**: Input = directory/list of files + level (and optional threshold); output = similar groups or pairs (JSON/text).

### 4.2 C++ (optional later)

- **Dependencies**: OpenCV (features + histogram); optional libpHash or custom hash.
- **Use**: Same binary as FileTimeFixer; e.g. `--find-similar`; levels 1–3 via threshold and optional feature matching.

### 4.3 With time-fix workflow

- Similar-image step **does not** change EXIF/file times; it only groups/lists.
- Typical flow: run similar-image tool → user/script decides keep/delete → run FileTimeFixer on kept files.

---

## 5. Technology summary

| Capability | Tech | Use |
|------------|------|-----|
| Perceptual hash | imagehash (Py) / libpHash (C++) | Levels 1–2, fast similarity |
| Decode/resize | Pillow (Py) / OpenCV or Exiv2 (C++) | Normalize before hash/features |
| Histogram | OpenCV / custom | Level 2 auxiliary |
| Local features | OpenCV SIFT/ORB | Level 3, same scene |
| Vector similarity | numpy (Py) / dot (C++) | Level 3 embeddings |
| Deep features | PyTorch + CNN (Py) | Level 3 semantic (optional) |

---

## 6. Suggested implementation order

1. **Phase 1**: Python with **imagehash** for levels 1–2, `--similarity-level` and optional `--similarity-threshold`, output groups or pairs.
2. **Phase 2**: Add OpenCV histogram or ORB for level 2 refinement and level 3 (match count).
3. **Phase 3** (optional): Embedding-based semantic similarity for level 3, or C++ OpenCV + hash in the main binary.

This yields **configurable-level similar image detection** that stays decoupled from and composable with the time-fix workflow.
