# 相似图片识别 (Similar Images)

在指定目录下扫描图片，按**相似度等级**识别相似图片并输出分组结果。与 [FileTimeFixer](../README.md) 的时间修复功能解耦，可单独使用或配合使用（先找相似图再决定保留/去重，再跑时间修复）。

设计说明与等级定义见 [docs/ImageSimilarityDesign.md](../docs/ImageSimilarityDesign.md)。

---

## 目录结构

```
similar_images/
├── README.md           # 本文件
├── requirements.txt    # Python 依赖
├── pyproject.toml      # 可选，pip install -e . 安装后命令行入口
└── similar_images/     # 包源码
    ├── __init__.py
    ├── similarity.py   # 感知哈希与等级阈值
    └── cli.py         # 命令行入口
```

---

## 功能概览

1. **单目录**：**递归**扫描目录下所有图片，输出相似图片对及相似度、路径。**相似度 100%** 的视为**重复图片**，在总结中统计并提示是否删除（仅 100% 重复可被删除，每组保留 EXIF 最新的一张）。
2. **双目录**：给定两个目录，在指定等级下对比，输出两目录间相似图片的**文件路径**（及相似度）。
3. **两图**：给定两张图片，输出这两张图片的**相似度**。

---

## 安装与运行

```bash
cd similar_images
pip install -r requirements.txt
# 可选：pip install -e .  之后可直接用 find-similar 命令
```

### 1. 单目录：递归扫描、输出相似图片及重复提示

默认**递归**搜索该目录及子目录下所有图片；输出每对相似度及路径，**相似度 100%** 的会标记为 `[duplicate]`。结束时会输出**总结**（相似对数、重复对数/组数），并**询问是否删除重复图片**（仅 100% 相同的会进入删除流程，每组保留 EXIF 时间最新的一张）。加 **`--yes`** 则直接删除不询问。

```bash
# 目录路径与选项之间要有空格，例如 "F:\Photos\DCIM" --verbose
python -m similar_images.cli <目录路径>
# 不递归子目录（仅当前目录）
python -m similar_images.cli --no-recursive <目录路径>
# 指定等级与阈值
python -m similar_images.cli --level 1 <目录路径>
python -m similar_images.cli --level 2 --threshold 12 <目录路径>
# 输出 JSON（含 results、summary 及 duplicate 统计）
python -m similar_images.cli --json <目录路径>
# 显示进度条、百分比及当前正在比较的图片
python -m similar_images.cli --verbose <目录路径>
python -m similar_images.cli -v <目录路径>
# 删除重复图片且不询问
python -m similar_images.cli --yes <目录路径>
```

### 2. 双目录：对比两目录下相似图片，输出路径

```bash
python -m similar_images.cli <目录1> <目录2>
python -m similar_images.cli --level 2 <目录1> <目录2>
python -m similar_images.cli --json <目录1> <目录2>
# 显示进度条与当前比较的文件
python -m similar_images.cli --verbose <目录1> <目录2>
```

### 3. 两图：输出两张图片的相似度

```bash
python -m similar_images.cli <图片1> <图片2>
python -m similar_images.cli --json <图片1> <图片2>
```

若已 `pip install -e .`，可直接使用 `find-similar` 命令，参数同上（1 个目录 / 2 个目录 / 2 个图片）。

---

## 相似度等级

| 等级 | 名称   | 说明 |
|------|--------|------|
| 1    | 严格   | 几乎同一张图（压缩、小幅缩放） |
| 2    | 中等   | 同一张图的不同尺寸/裁剪/调色 |
| 3    | 宽松   | 同场景不同构图（当前为放宽的哈希，后续可接特征匹配） |

---

## 进度与日志（目录模式）

使用 `--verbose` / `-v` 时：

- **进度条**：先显示「Hashing」阶段（计算每张图的感知哈希），再显示「Comparing」阶段（两两比较），带百分比。
- **当前文件**：进度条右侧显示当前正在处理的图片文件名；比较阶段显示当前一对文件名。

仅在选择单目录或双目录时生效，两图模式无进度条。

---

## 重复图片（100% 相似）与删除提示

单目录扫描结束后，程序会统计**相似度 100%** 的图片对（视为重复），在总结中输出「Duplicate (100% similar): N pair(s) in G group(s)」，并**询问是否删除重复图片**。只有 100% 相同的图片才会进入此删除流程；每组重复图中会保留 **EXIF 时间最新** 的一张，其余列入可删除列表。回答 `y` 后执行删除；加 **`--yes`** 则不等询问直接删除。

---

## 按 EXIF 时间去重（仅单目录，所有相似对）

当多张图片相似度很高时，可通过 `--dedupe` 按 EXIF 拍摄时间只保留一张、删除其余（针对**当前等级下所有相似对**，不仅 100%）：

- **`--dedupe keep-newer`**：保留 EXIF 时间**最新**的那张，删除其余（时间更早或无 EXIF 的会被删）。
- **`--dedupe keep-older`**：保留 EXIF 时间**最早**的那张，删除其余（时间更新的会被删）。

无 EXIF 的图片视为“最早”。默认会询问确认；加 **`--yes` / `-y`** 则直接执行不询问。

```bash
python -m similar_images.cli --dedupe keep-newer <目录>
python -m similar_images.cli --dedupe keep-older -y <目录>
```

---

## 将相似图片拷贝到当前目录

使用 **`--copy-similar`** 时，会把本次识别到的所有相似图片（出现在任意相似对中的文件）拷贝到**程序当前工作目录**下的 `similar_images_<时间戳>` 目录（时间戳格式 `YYYYMMDD_HHMMSS`）。单目录与双目录模式均支持；文件名冲突时自动加 `_2`、`_3` 等。

```bash
python -m similar_images.cli --copy-similar <目录>
python -m similar_images.cli --copy-similar <目录1> <目录2>
```

---

## 从文件读取比较对（-r）

使用 **`-r` / `--read-from-file`** 指定一个文本文件，文件中每行写两个图片路径，用 **`, `**（逗号+空格）分隔，程序会逐对比较并输出相似度。

- 空行和以 `#` 开头的行会忽略。
- 比较过程中会向**标准输出**滚动打印当前正在比较的两个文件路径（`[序号/总数] Comparing: path1 <-> path2`），便于观察进度；最终比较结果再统一输出。使用 **`--log-file <文件>`** 时，相同内容会同时追加写入该日志文件。
- 比较结果会按行输出；可用 **`--level`** 控制“相似”的阈值，输出中会标记 `[similar]`。
- 比较结束后会输出**总结**：总比较对数、相似对数；若执行了删除则还会输出删除的文件数。使用 `--log-file` 时总结也会写入日志。使用 `--json` 时输出为 `{"results": [...], "summary": {"total_compared", "similar_count", "deleted_count"}}`。

**`--keep-in-line first` / `--keep-in-line second`**：对于被判定为相似的每一对，保留该行中的**前一个**或**后一个**路径对应的文件，并**删除**另一个。默认会询问确认；加 **`--yes`** 则直接删除不询问。

```bash
# 仅比较并输出结果
python -m similar_images.cli -r pairs.txt
python -m similar_images.cli -r pairs.txt --level 1 --json

# 相似对中保留每行前一个，删除后一个（会询问）
python -m similar_images.cli -r pairs.txt --keep-in-line first

# 相似对中保留每行后一个，删除前一个，且不询问
python -m similar_images.cli -r pairs.txt --keep-in-line second -y

# 比较状态同时写入日志文件
python -m similar_images.cli -r pairs.txt --log-file compare.log
```

文本文件示例（`pairs.txt`）：

```
/path/to/a.jpg, /path/to/b.jpg
C:\Photos\img1.png, C:\Photos\img2.png
```

---

## 依赖

- **Python** ≥ 3.9
- **Pillow**：读图
- **imagehash**：感知哈希（pHash/dHash）
- **tqdm**：进度条（`--verbose` 时使用）
- 等级 3 或直方图辅助可选：**opencv-python**、**numpy**（见 requirements.txt 注释）
