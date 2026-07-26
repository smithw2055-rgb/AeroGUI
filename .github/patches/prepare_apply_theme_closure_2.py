from pathlib import Path

path = Path('.github/patches/apply_theme_objectwriter_closure.py')
text = path.read_text(encoding='utf-8')
old = '''replace_once(
    "samples/ControlGallery/GalleryRuntime.cpp",
    "runtime.Metadata().DependencyProperties())",
    "*runtime.MetadataRuntime())",
)
'''
new = '''replace_once(
    "samples/ControlGallery/GalleryRuntime.cpp",
    "Metadata().DependencyProperties())",
    "*runtime.MetadataRuntime())",
)
'''
if text.count(old) != 1:
    raise RuntimeError(f'expected one gallery runtime transform, found {text.count(old)}')
path.write_text(text.replace(old, new, 1), encoding='utf-8')
