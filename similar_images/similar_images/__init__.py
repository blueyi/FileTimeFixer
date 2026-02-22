"""Similar image detection by perceptual hash and configurable similarity level."""

from .similarity import (
    LEVEL_DEFAULTS,
    find_similar_groups,
    find_similar_pairs_with_scores,
    compare_dirs,
    compare_two_images,
    similarity_score,
    pairs_to_groups,
    get_files_to_delete_from_groups,
)

__all__ = [
    "find_similar_groups",
    "find_similar_pairs_with_scores",
    "compare_dirs",
    "compare_two_images",
    "similarity_score",
    "pairs_to_groups",
    "get_files_to_delete_from_groups",
    "LEVEL_DEFAULTS",
]
