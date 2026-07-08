import os
import re
import fnmatch
import json
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pathlib import Path

try:
    import zipfile
    try:
        import zlib
        ZIP_COMPRESSION = zipfile.ZIP_DEFLATED
    except ImportError:
        ZIP_COMPRESSION = zipfile.ZIP_STORED
    ZIP_AVAILABLE = True
except ImportError:
    ZIP_AVAILABLE = False
    ZIP_COMPRESSION = None


class ProjectGatherer:
    """Collects project files into a combined text file or a zip archive."""

    SKIP_DIRS = {'.venv', 'venv', 'env', '.env', '__pycache__', '.git', '.pytest_cache',
                 '.mypy_cache', 'build', 'dist', '*.egg-info', 'migrations', 'examples',
                 '.import', '.mono', '.addons', '.godot'}

    SKIP_FILES = {'export_presets.cfg', 'override.cfg', 'input_map.tres',
                  'default_env.tres', '.gdignore'}

    def __init__(self, root_dir, file_types=None, include_tree=False,
                 ignore_patterns=None, debug=False, allow_dot_root=False,
                 append_tree=False, include_patterns=None):
        self.root_dir = Path(root_dir).expanduser().resolve()
        self.project_name = self.root_dir.name
        self.output_file = Path(f"{self.project_name}-project.txt")
        self.file_types = file_types or ["gd"]
        self.include_tree = include_tree
        self.ignore_patterns = ignore_patterns or []
        self.debug = debug
        self.allow_dot_root = allow_dot_root
        self.append_tree = append_tree
        self.include_patterns = include_patterns or []

        if self.debug:
            print(f"Initialized gatherer with:")
            print(f"  Root directory: {self.root_dir}")
            print(f"  Project name: {self.project_name}")
            print(f"  Output file: {self.output_file}")
            print(f"  File types: {self.file_types}")
            print(f"  Include tree: {self.include_tree}")
            print(f"  Ignore patterns: {self.ignore_patterns}")
            print(f"  Include patterns: {self.include_patterns}")
            print(f"  Allow dot root: {self.allow_dot_root}")
            print(f"  Append tree: {self.append_tree}")

    def is_explicitly_included(self, path):
        """Return True when a path matches a forced-inclusion pattern."""
        try:
            rel_path = path.relative_to(self.root_dir)
        except ValueError:
            return False

        rel_path_str = str(rel_path).replace('\\', '/')

        for pattern in self.include_patterns:
            clean_pattern = pattern.rstrip('/').replace('\\', '/')
            if fnmatch.fnmatch(rel_path_str, clean_pattern) or fnmatch.fnmatch(rel_path_str, clean_pattern + '/*'):
                if self.debug:
                    print(f"Path '{rel_path_str}' matches include pattern '{pattern}'. Forcing inclusion.")
                return True
        return False

    def should_skip(self, path):
        """Return True when a path should be excluded from the gather."""
        if self.is_explicitly_included(path):
            return False

        rel_path = path.relative_to(self.root_dir)
        rel_parts = rel_path.parts

        if self.allow_dot_root and str(rel_path) == "." and path.name.startswith('.'):
            if self.debug:
                print(f"Not skipping root directory '{path}' even though it starts with a dot")
            return False

        if '.godot' in rel_parts:
            if self.debug:
                print(f"Skipping '{path}': Path contains .godot")
            return True

        if any(part.startswith('.') for part in rel_parts):
            if self.debug:
                print(f"Skipping '{path}': Path contains hidden directory")
            return True

        if any(part in self.SKIP_DIRS for part in rel_parts):
            if self.debug:
                print(f"Skipping '{path}': Path contains directory in SKIP_DIRS")
            return True

        if path.name in self.SKIP_FILES:
            if self.debug:
                print(f"Skipping '{path}': Filename is in SKIP_FILES")
            return True

        rel_str = str(rel_path).replace('\\', '/')
        for pattern in self.ignore_patterns:
            norm = pattern.replace('\\', '/').rstrip('/')
            if (fnmatch.fnmatch(path.name, pattern)
                    or fnmatch.fnmatch(rel_str, norm)
                    or rel_str == norm
                    or rel_str.startswith(norm + '/')):
                if self.debug:
                    print(f"Skipping '{path}': Matches ignore pattern '{pattern}'")
                return True

        return False

    def _parse_gitignore(self, root_dir):
        """Parse a .gitignore file into compiled regex patterns."""
        gitignore_path = Path(root_dir) / '.gitignore'
        if not gitignore_path.exists():
            return []

        patterns = []
        with open(gitignore_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue

                negated = line.startswith('!')
                if negated:
                    line = line[1:]

                pattern = re.escape(line)
                pattern = pattern.replace(r'\*\*', '.*')
                pattern = pattern.replace(r'\*', '[^/]*')
                pattern = pattern.replace(r'\?', '[^/]')

                if pattern.startswith('/'):
                    pattern = '^' + pattern[1:]
                else:
                    pattern = '(^|/)' + pattern

                if pattern.endswith(r'\/'):
                    pattern = pattern[:-2] + '(/.*)?$'
                else:
                    pattern = pattern + '(/.*)?$'

                patterns.append((re.compile(pattern), negated))

        return patterns

    def _get_default_ignore_patterns(self):
        """Return the built-in ignore patterns as compiled regex tuples."""
        default_patterns = [
            r'(^|/)__pycache__(/.*)?$',
            r'(^|/)\.pytest_cache(/.*)?$',
            r'(^|/)\.coverage$',
            r'(^|/)\.eggs(/.*)?$',
            r'(^|/)\.Python$',
            r'(^|/)build(/.*)?$',
            r'(^|/)develop-eggs(/.*)?$',
            r'(^|/)dist(/.*)?$',
            r'(^|/)downloads(/.*)?$',
            r'(^|/)\.tox(/.*)?$',
            r'(^|/)\.venv(/.*)?$',
            r'(^|/)venv(/.*)?$',
            r'(^|/)ENV(/.*)?$',
            r'(^|/)env(/.*)?$',
            r'(^|/)\.env(/.*)?$',
            r'(^|/)\.python-version$',
            r'(^|/)pyenv\.cfg$',
            r'(^|/)pip-log\.txt$',
            r'(^|/)pip-delete-this-directory\.txt$',
            r'(^|/).*\.pyc$',
            r'(^|/).*\.pyo$',
            r'(^|/).*\.pyd$',
            r'(^|/).*\.so$',
            r'(^|/).*\.dylib$',
            r'(^|/).*\.egg$',
            r'(^|/).*\.egg-info(/.*)?$',
            r'(^|/)\.installed\.cfg$',
            r'(^|/)site-packages(/.*)?$',
            r'(^|/)node_modules(/.*)?$',
            r'(^|/)npm-debug\.log.*$',
            r'(^|/)yarn-debug\.log.*$',
            r'(^|/)yarn-error\.log.*$',
            r'(^|/)\.npm(/.*)?$',
            r'(^|/)\.yarn(/.*)?$',
            r'(^|/)\.yarn-integrity$',
            r'(^|/)\.next(/.*)?$',
            r'(^|/)\.nuxt(/.*)?$',
            r'(^|/)\.cache(/.*)?$',
            r'(^|/)\.parcel-cache(/.*)?$',
            r'(^|/)\.output(/.*)?$',
            r'(^|/)dist(/.*)?$',
            r'(^|/)\.env\.local$',
            r'(^|/)\.env\.development$',
            r'(^|/)\.env\.test$',
            r'(^|/)\.env\.production$',
            r'(^|/)coverage(/.*)?$',
            r'(^|/)\.nyc_output(/.*)?$',
            r'(^|/)\.idea(/.*)?$',
            r'(^|/)\.vscode(/.*)?$',
            r'(^|/)\.vs(/.*)?$',
            r'(^|/)\.spyderproject$',
            r'(^|/)\.spyproject$',
            r'(^|/)\.ropeproject(/.*)?$',
            r'(^|/)\.classpath$',
            r'(^|/)\.project$',
            r'(^|/)\.settings(/.*)?$',
            r'(^|/)\.godot(/.*)?$',
            r'(^|/)\.import$',
            r'(^|/).*\.import$',
            r'(^|/).*\.uid$',
            r'(^|/).*\.tmp$',
            r'(^|/).*\.bin$',
            r'(^|/)\.editorconfig$',
            r'(^|/)\.git(/.*)?$',
            r'(^|/)\.gitattributes$',
            r'(^|/)\.gitignore$',
            r'(^|/)\.hg(/.*)?$',
            r'(^|/)\.svn(/.*)?$',
            r'(^|/)\.DS_Store$',
            r'(^|/)Thumbs\.db$',
            r'(^|/)desktop\.ini$',
        ]
        return [(re.compile(pattern), False) for pattern in default_patterns]

    def _should_ignore_for_tree(self, path, root_dir, ignore_patterns):
        """Return True when a path should be hidden from the visual tree."""
        path_obj = Path(path)
        if self.is_explicitly_included(path_obj):
            return False

        try:
            rel_path = os.path.relpath(path, root_dir)
            rel_path = rel_path.replace('\\', '/')
            if rel_path == '.':
                return False
        except ValueError:
            return True

        ignore = False
        for pattern, negated in ignore_patterns:
            if pattern.search(rel_path):
                ignore = not negated
        return ignore

    def generate_tree_representation(self, no_gitignore=False, no_default_ignore=False):
        """Build a text representation of the project directory tree."""
        directory = str(self.root_dir)
        ignore_patterns = [] if no_default_ignore else self._get_default_ignore_patterns()

        if not no_gitignore:
            gitignore_patterns = self._parse_gitignore(directory)
            ignore_patterns.extend(gitignore_patterns)

        tree_output = [os.path.basename(directory)]

        def _generate_tree_recursive(dir_path, prefix=""):
            try:
                items = sorted(os.listdir(dir_path), key=lambda x: (not os.path.isdir(os.path.join(dir_path, x)), x.lower()))
            except PermissionError:
                tree_output.append(f"{prefix}└── [Permission denied]")
                return
            except FileNotFoundError:
                tree_output.append(f"{prefix}└── [Directory not found]")
                return

            visible_items = [item for item in items if not self._should_ignore_for_tree(os.path.join(dir_path, item), directory, ignore_patterns)]

            for i, item in enumerate(visible_items):
                item_path = os.path.join(dir_path, item)
                is_last = i == len(visible_items) - 1
                if is_last:
                    branch = "└── "
                    new_prefix = prefix + "    "
                else:
                    branch = "├── "
                    new_prefix = prefix + "│   "

                tree_output.append(f"{prefix}{branch}{item}")

                if os.path.isdir(item_path):
                    _generate_tree_recursive(item_path, new_prefix)

        _generate_tree_recursive(directory)
        return "\n".join(tree_output)

    def gather_files(self):
        """Write all matching files into a single combined text file."""
        if self.debug:
            print(f"\nGathering files with types: {self.file_types}")

        files_processed = 0
        files_included = 0

        with open(self.output_file, 'w', encoding='utf-8') as outfile:
            for file_type in self.file_types:
                file_pattern = f"*.{file_type}"
                if self.debug:
                    print(f"\nLooking for files matching: {file_pattern}")

                matching_files = list(self.root_dir.rglob(file_pattern))
                if self.debug:
                    print(f"Found {len(matching_files)} files matching pattern {file_pattern}")
                    if len(matching_files) > 0:
                        print("First 5 files found:")
                        for f in sorted(matching_files)[:5]:
                            print(f"  - {f}")

                for file in sorted(matching_files):
                    files_processed += 1
                    rel_path = file.relative_to(self.root_dir)

                    if self.should_skip(file):
                        continue

                    try:
                        with open(file, 'r', encoding='utf-8') as infile:
                            content = infile.read().strip()
                    except Exception as e:
                        print(f"Error processing {file}: {e}")
                        content = ""

                    if self.include_tree or content:
                        if self.debug:
                            print(f"Including: {rel_path}")
                        outfile.write(f"\n#{'='*80}\n# {rel_path} ({file_type})\n#{'='*80}\n\n")
                        if content:
                            outfile.write(content + "\n\n")
                        files_included += 1

            if self.include_tree:
                for dirpath, dirnames, filenames in os.walk(self.root_dir):
                    dir_path = Path(dirpath)
                    if self.should_skip(dir_path):
                        continue

                    dir_rel = dir_path.relative_to(self.root_dir)
                    if dir_rel == Path('.'):
                        continue

                    has_valid_files = any(
                        Path(f).suffix.lstrip('.') in self.file_types
                        and not self.should_skip(dir_path / f)
                        for f in filenames
                    )
                    if not has_valid_files:
                        outfile.write(f"\n#{'='*80}\n# {dir_rel}/ (empty or no matching files)\n#{'='*80}\n\n")

            if self.append_tree:
                tree_representation = self.generate_tree_representation()
                outfile.write(f"\n\n#{'='*80}\n# PROJECT DIRECTORY TREE\n#{'='*80}\n\n")
                outfile.write(tree_representation)

        if self.debug:
            print(f"\nSummary:")
            print(f"  Files processed: {files_processed}")
            print(f"  Files included: {files_included}")
            print(f"  Output file: {self.output_file}")
            print(f"  Tree appended: {self.append_tree}")

        return files_included > 0

    def gather_zip(self):
        """Write all matching files into a .zip archive preserving relative paths."""
        if not ZIP_AVAILABLE:
            raise RuntimeError("The zipfile module is unavailable; zip output is disabled.")

        if self.debug:
            print(f"\nGathering files into zip with types: {self.file_types}")

        files_processed = 0
        files_included = 0
        seen = set()

        with zipfile.ZipFile(self.output_file, 'w', ZIP_COMPRESSION) as archive:
            for file_type in self.file_types:
                file_pattern = f"*.{file_type}"
                if self.debug:
                    print(f"\nLooking for files matching: {file_pattern}")

                matching_files = list(self.root_dir.rglob(file_pattern))
                if self.debug:
                    print(f"Found {len(matching_files)} files matching pattern {file_pattern}")

                for file in sorted(matching_files):
                    files_processed += 1
                    if self.should_skip(file):
                        continue

                    rel_path = file.relative_to(self.root_dir)
                    arcname = str(rel_path).replace('\\', '/')
                    if arcname in seen:
                        continue
                    seen.add(arcname)

                    try:
                        archive.write(file, arcname=arcname)
                        files_included += 1
                        if self.debug:
                            print(f"Including: {rel_path}")
                    except Exception as e:
                        print(f"Error processing {file}: {e}")

            if self.append_tree:
                tree_representation = self.generate_tree_representation()
                archive.writestr(f"{self.project_name}-tree.txt", tree_representation)

        if self.debug:
            print(f"\nSummary:")
            print(f"  Files processed: {files_processed}")
            print(f"  Files included: {files_included}")
            print(f"  Output file: {self.output_file}")
            print(f"  Tree appended: {self.append_tree}")

        return files_included > 0


class ProjectGathererApp(tk.Tk):
    """Tkinter front end for configuring and running the project gatherer."""

    def __init__(self):
        super().__init__()
        self.title("Project Gatherer GUI")
        self.geometry("900x950")

        self.style = ttk.Style()
        self.style.configure(".", font=('Segoe UI', 10))
        self.style.configure("Treeview", font=('Segoe UI', 11), rowheight=25, indent=15)
        self.style.configure("Treeview.Heading", font=('Segoe UI', 11, 'bold'))
        self.style.configure("TCheckbutton", font=('Segoe UI', 10))

        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.lastrun_file = Path(self.script_dir) / ".lastrun"

        self.source_dir = tk.StringVar(value=self.script_dir)
        self.output_dir = tk.StringVar(value=self.script_dir)

        default_name = f"{os.path.basename(self.script_dir)}-project.txt"
        self.output_filename = tk.StringVar(value=default_name)
        self.output_format = tk.StringVar(value="txt")

        self.ext_gd = tk.BooleanVar(value=True)
        self.ext_tscn = tk.BooleanVar(value=True)
        self.ext_tres = tk.BooleanVar(value=True)

        self.ext_cfg = tk.BooleanVar(value=False)
        self.ext_json = tk.BooleanVar(value=False)
        self.ext_ini = tk.BooleanVar(value=False)
        self.ext_txt = tk.BooleanVar(value=False)
        self.ext_md = tk.BooleanVar(value=False)
        self.include_project_file = tk.BooleanVar(value=False)

        self.include_tree = tk.BooleanVar(value=False)
        self.append_tree = tk.BooleanVar(value=False)
        self.debug_mode = tk.BooleanVar(value=False)
        self.allow_dot_root = tk.BooleanVar(value=False)

        self.load_status = tk.StringVar(value="")

        self.tree_items = {}
        self.path_state_cache = {}

        self.CHECKED = "☑ "
        self.UNCHECKED = "☐ "

        self._setup_ui()
        self._load_lastrun()
        self._populate_tree(self.source_dir.get())

    def _setup_ui(self):
        """Construct and lay out all widgets."""
        main_frame = ttk.Frame(self, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        src_frame = ttk.LabelFrame(main_frame, text="Source Configuration", padding="5")
        src_frame.pack(fill=tk.X, pady=5)

        ttk.Label(src_frame, text="Project Root:").grid(row=0, column=0, sticky="w", padx=5)
        ttk.Entry(src_frame, textvariable=self.source_dir, width=60).grid(row=0, column=1, padx=5)
        ttk.Button(src_frame, text="Browse...", command=self._browse_source).grid(row=0, column=2, padx=5)

        status_frame = ttk.Frame(src_frame)
        status_frame.grid(row=1, column=0, columnspan=3, sticky="ew", pady=(5, 0))

        ttk.Label(status_frame, textvariable=self.load_status, foreground="blue", font=('Segoe UI', 9, 'italic')).pack(side=tk.LEFT, padx=5)
        ttk.Button(status_frame, text="Reset to Defaults", command=self._reset_defaults, width=15).pack(side=tk.RIGHT, padx=5)

        ext_frame = ttk.LabelFrame(main_frame, text="File Types", padding="5")
        ext_frame.pack(fill=tk.X, pady=5)

        common_frame = ttk.Frame(ext_frame)
        common_frame.pack(fill=tk.X)

        for text, var in [("GD Script (.gd)", self.ext_gd),
                          ("Scene (.tscn)", self.ext_tscn),
                          ("Resource (.tres)", self.ext_tres)]:
            cb = ttk.Checkbutton(common_frame, text=text, variable=var)
            cb.pack(side=tk.LEFT, padx=10)
            var.trace_add("write", self._refresh_tree)

        self.adv_btn = ttk.Button(ext_frame, text="Show Advanced Options ▼", command=self._toggle_advanced)
        self.adv_btn.pack(fill=tk.X, pady=(5, 0))

        self.adv_frame = ttk.Frame(ext_frame, padding="5")

        adv_grid = ttk.Frame(self.adv_frame)
        adv_grid.pack(fill=tk.X)

        adv_opts = [
            ("Config (.cfg)", self.ext_cfg),
            ("JSON (.json)", self.ext_json),
            ("INI (.ini)", self.ext_ini),
            ("Text (.txt)", self.ext_txt),
            ("Markdown (.md)", self.ext_md),
            ("Include .godot project file", self.include_project_file)
        ]

        for i, (text, var) in enumerate(adv_opts):
            cb = ttk.Checkbutton(adv_grid, text=text, variable=var)
            cb.grid(row=i // 3, column=i % 3, sticky="w", padx=10, pady=2)
            var.trace_add("write", self._refresh_tree)

        tree_frame = ttk.LabelFrame(main_frame, text="File Selection Tree", padding="5")
        tree_frame.pack(fill=tk.BOTH, expand=True, pady=5)

        ttk.Label(tree_frame, text="Single Click: Highlight/View | Double Click or Spacebar: Toggle Checkbox",
                  font=('Segoe UI', 9, 'italic'), foreground="gray").grid(row=0, column=0, sticky="w", padx=5)

        self.tree = ttk.Treeview(tree_frame, selectmode="browse", show="tree")
        ysb = ttk.Scrollbar(tree_frame, orient="vertical", command=self.tree.yview)
        xsb = ttk.Scrollbar(tree_frame, orient="horizontal", command=self.tree.xview)
        self.tree.configure(yscrollcommand=ysb.set, xscrollcommand=xsb.set)

        self.tree.grid(row=1, column=0, sticky="nsew")
        ysb.grid(row=1, column=1, sticky="ns")
        xsb.grid(row=2, column=0, sticky="ew")

        tree_frame.grid_columnconfigure(0, weight=1)
        tree_frame.grid_rowconfigure(1, weight=1)

        self.tree.bind("<Double-1>", self._toggle_selection_no_expand)
        self.tree.bind("<space>", self._toggle_selection_no_expand)

        opts_frame = ttk.LabelFrame(main_frame, text="Output Options", padding="5")
        opts_frame.pack(fill=tk.X, pady=5)

        ttk.Label(opts_frame, text="Output Folder:").grid(row=0, column=0, sticky="w", padx=5)
        ttk.Entry(opts_frame, textvariable=self.output_dir, width=50).grid(row=0, column=1, padx=5, sticky="ew")
        ttk.Button(opts_frame, text="Browse...", command=self._browse_output).grid(row=0, column=2, padx=5)

        ttk.Label(opts_frame, text="Filename:").grid(row=1, column=0, sticky="w", padx=5)
        ttk.Entry(opts_frame, textvariable=self.output_filename, width=50).grid(row=1, column=1, padx=5, sticky="ew")

        ttk.Label(opts_frame, text="Format:").grid(row=2, column=0, sticky="w", padx=5, pady=(5, 0))
        format_frame = ttk.Frame(opts_frame)
        format_frame.grid(row=2, column=1, sticky="w", padx=5, pady=(5, 0))

        ttk.Radiobutton(format_frame, text="Combined .txt", variable=self.output_format, value="txt").pack(side=tk.LEFT, padx=(0, 15))
        zip_radio = ttk.Radiobutton(format_frame, text="Zip archive (.zip)", variable=self.output_format, value="zip")
        zip_radio.pack(side=tk.LEFT)
        if not ZIP_AVAILABLE:
            zip_radio.configure(state="disabled")

        self.output_format.trace_add("write", self._on_format_change)

        opts_frame.grid_columnconfigure(1, weight=1)

        flags_frame = ttk.LabelFrame(main_frame, text="Flags", padding="5")
        flags_frame.pack(fill=tk.X, pady=5)

        ttk.Checkbutton(flags_frame, text="Include Tree (Empty files/folders)", variable=self.include_tree).pack(side=tk.LEFT, padx=10)
        ttk.Checkbutton(flags_frame, text="Append Visual Tree", variable=self.append_tree).pack(side=tk.LEFT, padx=10)
        ttk.Checkbutton(flags_frame, text="Allow Dot Root", variable=self.allow_dot_root).pack(side=tk.LEFT, padx=10)
        ttk.Checkbutton(flags_frame, text="Debug Mode", variable=self.debug_mode).pack(side=tk.LEFT, padx=10)

        action_frame = ttk.Frame(main_frame, padding="10")
        action_frame.pack(fill=tk.X)
        ttk.Button(action_frame, text="GATHER FILES", command=self._run_gatherer).pack(fill=tk.X, ipady=10)

    def _toggle_advanced(self):
        """Show or hide the advanced file type options."""
        if self.adv_frame.winfo_viewable():
            self.adv_frame.pack_forget()
            self.adv_btn.configure(text="Show Advanced Options ▼")
        else:
            self.adv_frame.pack(fill=tk.X)
            self.adv_btn.configure(text="Hide Advanced Options ▲")

    def _get_active_extensions(self):
        """Return the list of currently enabled file extensions."""
        exts = []
        if self.ext_gd.get(): exts.append("gd")
        if self.ext_tscn.get(): exts.append("tscn")
        if self.ext_tres.get(): exts.append("tres")
        if self.ext_cfg.get(): exts.append("cfg")
        if self.ext_json.get(): exts.append("json")
        if self.ext_ini.get(): exts.append("ini")
        if self.ext_txt.get(): exts.append("txt")
        if self.ext_md.get(): exts.append("md")
        if self.include_project_file.get(): exts.append("godot")
        return exts

    def _default_filename(self):
        """Return the default output filename for the active format."""
        base = os.path.basename(self.source_dir.get() or self.script_dir)
        ext = "zip" if self.output_format.get() == "zip" else "txt"
        return f"{base}-project.{ext}"

    def _on_format_change(self, *args):
        """Keep the output filename extension in sync with the selected format."""
        name = self.output_filename.get()
        stem = re.sub(r'\.(txt|zip)$', '', name, flags=re.IGNORECASE)
        if not stem:
            stem = os.path.basename(self.source_dir.get() or self.script_dir) + "-project"
        ext = "zip" if self.output_format.get() == "zip" else "txt"
        self.output_filename.set(f"{stem}.{ext}")

    def _browse_source(self):
        """Prompt for a project root and refresh the selection tree."""
        path = filedialog.askdirectory()
        if path:
            self.source_dir.set(path)
            if not self.output_dir.get():
                self.output_dir.set(path)

            project_name = os.path.basename(path)
            ext = "zip" if self.output_format.get() == "zip" else "txt"
            self.output_filename.set(f"{project_name}-project.{ext}")

            self._populate_tree(path)

    def _browse_output(self):
        """Prompt for an output folder."""
        path = filedialog.askdirectory()
        if path:
            self.output_dir.set(path)

    def _refresh_tree(self, *args):
        """Repopulate the tree when source or file types change."""
        src = self.source_dir.get()
        if src and os.path.exists(src):
            self._populate_tree(src)

    def _save_tree_state(self):
        """Recursively save the expanded and checked state of current items."""
        for item_id in self.tree_items:
            data = self.tree_items[item_id]
            path = data["path"]
            is_open = self.tree.item(item_id, "open")
            is_checked = data["checked"]
            self.path_state_cache[path] = {"open": is_open, "checked": is_checked}

    def _populate_tree(self, root_path):
        """Rebuild the file selection tree rooted at the given path."""
        self._save_tree_state()

        self.tree.delete(*self.tree.get_children())
        self.tree_items.clear()

        if not os.path.exists(root_path):
            return

        active_exts = set(self._get_active_extensions())

        root_cache = self.path_state_cache.get(root_path, {"open": True, "checked": True})

        root_node = self.tree.insert("", "end", text=f"{self.CHECKED if root_cache['checked'] else self.UNCHECKED}{root_path}", open=root_cache["open"])
        self.tree_items[root_node] = {"path": root_path, "checked": root_cache["checked"], "type": "dir"}

        self._add_nodes(root_node, root_path, active_exts)

    def _add_nodes(self, parent_id, current_path, active_exts):
        """Recursively add child nodes for files and directories."""
        try:
            items = sorted(os.listdir(current_path))
        except PermissionError:
            return

        for item in items:
            full_path = os.path.join(current_path, item)

            if item == '.godot':
                continue

            if item in ProjectGatherer.SKIP_DIRS or item in ProjectGatherer.SKIP_FILES:
                continue

            is_dir = os.path.isdir(full_path)

            if not is_dir:
                ext = os.path.splitext(item)[1].lstrip('.').lower()
                if ext not in active_exts:
                    continue

            cache = self.path_state_cache.get(full_path, {"open": False, "checked": True})

            node_id = self.tree.insert(parent_id, "end", text=f"{self.CHECKED if cache['checked'] else self.UNCHECKED}{item}", open=cache["open"])
            self.tree_items[node_id] = {"path": full_path, "checked": cache["checked"], "type": "dir" if is_dir else "file"}

            if is_dir:
                self._add_nodes(node_id, full_path, active_exts)

    def _toggle_selection_no_expand(self, event=None):
        """Toggle the checkbox of the selected item without expanding it."""
        selected_item = self.tree.selection()
        if not selected_item:
            return "break"

        for item_id in selected_item:
            data = self.tree_items.get(item_id)
            if not data:
                continue

            new_state = not data["checked"]
            self._toggle_node(item_id, new_state)

        return "break"

    def _toggle_node(self, item_id, state):
        """Set the checked state of a node and propagate it to children."""
        data = self.tree_items[item_id]
        data["checked"] = state

        item_text = self.tree.item(item_id, "text")
        clean_text = item_text[2:]
        prefix = self.CHECKED if state else self.UNCHECKED
        self.tree.item(item_id, text=f"{prefix}{clean_text}")

        for child_id in self.tree.get_children(item_id):
            self._toggle_node(child_id, state)

        self.path_state_cache[data["path"]] = {"open": self.tree.item(item_id, "open"), "checked": state}

    def _generate_ignore_patterns(self):
        """Collect relative paths of unchecked items as ignore patterns."""
        ignore_list = []
        root_dir = self.source_dir.get()

        root_children = self.tree.get_children()
        if not root_children:
            return []

        real_root_id = root_children[0]

        def _process_node(node_id):
            data = self.tree_items[node_id]
            full_path = data["path"]

            if not data["checked"]:
                try:
                    rel_path = os.path.relpath(full_path, root_dir)
                    rel_path = rel_path.replace('\\', '/')
                    ignore_list.append(rel_path)
                except ValueError:
                    pass
                return

            for child_id in self.tree.get_children(node_id):
                _process_node(child_id)

        for child_id in self.tree.get_children(real_root_id):
            _process_node(child_id)

        return ignore_list

    def _save_lastrun(self):
        """Persist the current configuration and tree state to .lastrun."""
        self._save_tree_state()

        unchecked_paths = []
        expanded_paths = []

        root_path = self.source_dir.get()

        for path, state in self.path_state_cache.items():
            try:
                if path == root_path:
                    rel = "."
                else:
                    rel = os.path.relpath(path, root_path)

                rel = rel.replace('\\', '/')

                if not state["checked"]:
                    unchecked_paths.append(rel)
                if state["open"]:
                    expanded_paths.append(rel)
            except ValueError:
                continue

        config = {
            "source_dir": self.source_dir.get(),
            "output_dir": self.output_dir.get(),
            "output_filename": self.output_filename.get(),
            "output_format": self.output_format.get(),
            "extensions": {
                "gd": self.ext_gd.get(),
                "tscn": self.ext_tscn.get(),
                "tres": self.ext_tres.get(),
                "cfg": self.ext_cfg.get(),
                "json": self.ext_json.get(),
                "ini": self.ext_ini.get(),
                "txt": self.ext_txt.get(),
                "md": self.ext_md.get(),
                "godot": self.include_project_file.get()
            },
            "flags": {
                "include_tree": self.include_tree.get(),
                "append_tree": self.append_tree.get(),
                "allow_dot_root": self.allow_dot_root.get(),
                "debug_mode": self.debug_mode.get()
            },
            "tree_state": {
                "unchecked": unchecked_paths,
                "expanded": expanded_paths
            }
        }

        try:
            with open(self.lastrun_file, 'w') as f:
                json.dump(config, f, indent=4)
        except Exception as e:
            print(f"Failed to save .lastrun: {e}")

    def _load_lastrun(self):
        """Restore configuration and tree state from .lastrun if present."""
        if not self.lastrun_file.exists():
            self.load_status.set("Default configuration loaded.")
            return

        try:
            with open(self.lastrun_file, 'r') as f:
                config = json.load(f)

            self.source_dir.set(config.get("source_dir", self.script_dir))
            self.output_dir.set(config.get("output_dir", self.script_dir))
            self.output_filename.set(config.get("output_filename", "project.txt"))
            requested_format = config.get("output_format", "txt")
            if requested_format == "zip" and not ZIP_AVAILABLE:
                requested_format = "txt"
            self.output_format.set(requested_format)

            exts = config.get("extensions", {})
            self.ext_gd.set(exts.get("gd", True))
            self.ext_tscn.set(exts.get("tscn", True))
            self.ext_tres.set(exts.get("tres", True))
            self.ext_cfg.set(exts.get("cfg", False))
            self.ext_json.set(exts.get("json", False))
            self.ext_ini.set(exts.get("ini", False))
            self.ext_txt.set(exts.get("txt", False))
            self.ext_md.set(exts.get("md", False))
            self.include_project_file.set(exts.get("godot", False))

            flags = config.get("flags", {})
            self.include_tree.set(flags.get("include_tree", False))
            self.append_tree.set(flags.get("append_tree", False))
            self.allow_dot_root.set(flags.get("allow_dot_root", False))
            self.debug_mode.set(flags.get("debug_mode", False))

            tree_state = config.get("tree_state", {})
            unchecked = set(tree_state.get("unchecked", []))
            expanded = set(tree_state.get("expanded", []))

            self.path_state_cache = {}
            root = self.source_dir.get()

            all_saved_rels = unchecked.union(expanded)

            for rel in all_saved_rels:
                if rel == ".":
                    abs_path = root
                else:
                    abs_path = os.path.join(root, rel)

                abs_path = os.path.normpath(abs_path)

                is_checked = rel not in unchecked
                is_open = rel in expanded

                self.path_state_cache[abs_path] = {"checked": is_checked, "open": is_open}

            self.load_status.set(f"Loaded config from {self.lastrun_file.name}")

        except Exception as e:
            self.load_status.set(f"Error loading .lastrun: {e}")
            print(f"Error loading .lastrun: {e}")

    def _reset_defaults(self):
        """Delete the saved config and restore all defaults."""
        if self.lastrun_file.exists():
            try:
                os.remove(self.lastrun_file)
            except Exception:
                pass

        self.source_dir.set(self.script_dir)
        self.output_dir.set(self.script_dir)
        self.output_format.set("txt")
        default_name = f"{os.path.basename(self.script_dir)}-project.txt"
        self.output_filename.set(default_name)

        self.ext_gd.set(True)
        self.ext_tscn.set(True)
        self.ext_tres.set(True)
        self.ext_cfg.set(False)
        self.ext_json.set(False)
        self.ext_ini.set(False)
        self.ext_txt.set(False)
        self.ext_md.set(False)
        self.include_project_file.set(False)

        self.include_tree.set(False)
        self.append_tree.set(False)
        self.allow_dot_root.set(False)
        self.debug_mode.set(False)

        self.path_state_cache.clear()

        self.load_status.set("Configuration reset to defaults.")

        self._populate_tree(self.script_dir)

    def _run_gatherer(self):
        """Validate inputs and run the gather in the selected output format."""
        src = self.source_dir.get()
        out_dir = self.output_dir.get()
        fname = self.output_filename.get()

        if not src or not os.path.exists(src):
            messagebox.showerror("Error", "Invalid source directory.")
            return

        if not out_dir or not os.path.exists(out_dir):
            messagebox.showerror("Error", "Invalid output directory.")
            return

        use_zip = self.output_format.get() == "zip"
        if use_zip and not ZIP_AVAILABLE:
            messagebox.showerror("Error", "Zip output is unavailable on this system.")
            return

        self._save_lastrun()

        active_exts = self._get_active_extensions()
        ignore_patterns = self._generate_ignore_patterns()

        gatherer = ProjectGatherer(
            root_dir=src,
            file_types=active_exts,
            include_tree=self.include_tree.get(),
            ignore_patterns=ignore_patterns,
            debug=self.debug_mode.get(),
            allow_dot_root=self.allow_dot_root.get(),
            append_tree=self.append_tree.get()
        )

        gatherer.output_file = Path(out_dir) / fname

        try:
            success = gatherer.gather_zip() if use_zip else gatherer.gather_files()
            if success:
                messagebox.showinfo("Success", f"Files gathered into:\n{gatherer.output_file}")
            else:
                messagebox.showwarning("Result", "No matching files found based on criteria.")
        except Exception as e:
            messagebox.showerror("Error", f"An error occurred:\n{str(e)}")


if __name__ == "__main__":
    app = ProjectGathererApp()
    app.mainloop()