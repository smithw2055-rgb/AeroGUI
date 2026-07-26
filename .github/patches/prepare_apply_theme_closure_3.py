from pathlib import Path

path = Path('.github/patches/apply_theme_objectwriter_closure.py')
text = path.read_text(encoding='utf-8')
old = '''Do not add another theme DOM, resource wrapper, activation registry, or feature-local property system during that migration.
"""
'''
new = '''Migration invariant: `XamlObjectWriter`, metadata facets, and `Core::Value` remain the only object construction, member assignment, and resource-value pipeline. Theme code may adapt the resulting object graph, but must not introduce a parallel parser, writer, property system, or resource wrapper.
"""
'''
if text.count(old) != 1:
    raise RuntimeError(f'expected one document baseline tail, found {text.count(old)}')
path.write_text(text.replace(old, new, 1), encoding='utf-8')
