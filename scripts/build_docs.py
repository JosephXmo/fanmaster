#!/usr/bin/env python3
"""Build the independent Fanmaster reference site (never part of web/ assets)."""
import argparse
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import shutil
from string import Template
from urllib.parse import unquote, urlsplit

try:
    import markdown
except ImportError as exc:
    raise SystemExit('Install documentation dependencies: python -m pip install -r docs/requirements.txt') from exc

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / 'docs'


class Document(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids, self.links, self.text = set(), [], []
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            if attrs['id'] in self.ids:
                raise ValueError(f'Duplicate HTML id: {attrs["id"]}')
            self.ids.add(attrs['id'])
        for key in ('href', 'src'):
            if attrs.get(key):
                self.links.append(attrs[key])
    def handle_data(self, text):
        self.text.append(text)


def plain(fragment):
    parser = Document()
    parser.feed(fragment)
    return ' '.join(' '.join(parser.text).split())


def validate(output):
    documents = {}
    for path in output.glob('*.html'):
        parser = Document()
        parser.feed(path.read_text(encoding='utf-8'))
        documents[path.resolve()] = parser
    failures = []
    for path, document in documents.items():
        for link in document.links:
            url = urlsplit(html.unescape(link))
            if url.scheme or url.netloc:
                continue
            target = (path.parent / unquote(url.path)).resolve() if url.path else path
            if not target.is_relative_to(output.resolve()):
                failures.append(f'{path.name}: link escapes site: {link}')
            elif not target.is_file():
                failures.append(f'{path.name}: missing file: {link}')
            elif url.fragment and target in documents and unquote(url.fragment) not in documents[target].ids:
                failures.append(f'{path.name}: missing anchor: {link}')
    if failures:
        raise ValueError('\n'.join(failures))
    return len(documents)


def build(output):
    # Output must be a dedicated generated directory, outside deployable web assets.
    output = output.resolve()
    web = (ROOT / 'web').resolve()
    protected = (ROOT, DOCS, DOCS / 'content', DOCS / 'theme')
    if (output in protected or output.is_relative_to(web) or web.is_relative_to(output)
            or DOCS.is_relative_to(output) or output.is_relative_to(DOCS / 'content')
            or output.is_relative_to(DOCS / 'theme')):
        raise ValueError('Use a dedicated output directory outside web/ and documentation sources')
    config = json.loads((DOCS / 'site.json').read_text(encoding='utf-8'))
    pages = []
    for group in config['groups']:
        for page in group['pages']:
            pages.append({**page, 'group': group['title']})
    sources = {p['source'] for p in pages}
    if len(sources) != len(pages) or len({p['slug'] for p in pages}) != len(pages):
        raise ValueError('Duplicate source or page slug')
    if sources != {p.name for p in (DOCS / 'content').glob('*.md')}:
        raise ValueError('Every content source must be registered exactly once in docs/site.json')
    targets = {p['source']: p['slug'] + '.html' for p in pages}
    def rewrite(match):
        href = html.unescape(match.group(1))
        parsed = urlsplit(href)
        if not parsed.scheme and parsed.path.endswith('.md'):
            if parsed.path not in targets:
                raise ValueError(f'Unknown Markdown link: {href}')
            href = targets[parsed.path] + ('#' + parsed.fragment if parsed.fragment else '')
        return 'href="' + html.escape(href, quote=True) + '"'
    rendered, search = [], []
    for page in pages:
        source = (DOCS / 'content' / page['source']).read_text(encoding='utf-8')
        heading = re.search(r'^# (.+)$', source, flags=re.M)
        if not heading:
            raise ValueError(f'Missing page title: {page["source"]}')
        title = heading.group(1)
        source = source[:heading.start()] + source[heading.end():]
        md = markdown.Markdown(extensions=['fenced_code', 'tables', 'toc', 'sane_lists'],
            extension_configs={'toc': {'permalink': '#', 'toc_depth': '2-3'}})
        body = re.sub(r'href="([^"]+)"', rewrite, md.convert(source))
        # Responsive table wrappers retain native table semantics.
        body = body.replace('<table>', '<div class="table-scroll" tabindex="0" role="region" aria-label="Scrollable reference table"><table>').replace('</table>', '</table></div>')
        toc = re.sub(r'href="([^"]+)"', rewrite, md.toc)
        rendered.append({**page, 'heading': title, 'body': body, 'toc': toc})
        search.append({'title': page['title'], 'section': 'Introduction', 'url': page['slug']+'.html', 'text': page['description']})
        for segment in re.split(r'(?=<h[23] id=)', body):
            heading_match = re.match(r'<h[23] id="([^"]+)">(.+?)</h[23]>', segment, re.S)
            if heading_match:
                search.append({'title': page['title'], 'section': plain(heading_match.group(2)).rstrip(' #'),
                    'url': page['slug']+'.html#'+heading_match.group(1), 'text': plain(segment)})
    output.mkdir(parents=True, exist_ok=True)
    # Remove only previously generated HTML; never recursively erase an arbitrary output.
    marker = output / '.fanmaster-docs'
    if any(output.iterdir()) and not marker.exists():
        raise ValueError(f'Output is not a marked Fanmaster documentation build: {output}')
    for path in output.glob('*.html'):
        path.unlink()
    marker.write_text('Generated by scripts/build_docs.py\n', encoding='utf-8')
    shutil.copytree(DOCS / 'theme' / 'assets', output / 'assets', dirs_exist_ok=True)
    shutil.copytree(DOCS / 'content', output / 'sources', dirs_exist_ok=True)
    # JS assignment, rather than fetch(), keeps search available on file:// too.
    data = json.dumps(search, ensure_ascii=False).replace('<', '\\u003c')
    (output / 'assets' / 'search-index.js').write_text('window.FANMASTER_SEARCH = '+data+';\n', encoding='utf-8')
    template = Template((DOCS / 'theme' / 'page.html').read_text(encoding='utf-8'))
    esc = lambda value: html.escape(value, quote=True)
    for index, page in enumerate(rendered):
        navigation = []
        for group in config['groups']:
            navigation.append('<div class="nav-group"><p>'+esc(group['title'])+'</p>')
            for item in group['pages']:
                current = ' aria-current="page"' if item['slug'] == page['slug'] else ''
                navigation.append('<a href="'+item['slug']+'.html"'+current+'>'+esc(item['title'])+'</a>')
            navigation.append('</div>')
        def neighbor(offset, label):
            position = index + offset
            if not 0 <= position < len(rendered):
                return '<span></span>'
            item = rendered[position]
            return '<a href="'+item['slug']+'.html"><span>'+label+'</span><strong>'+esc(item['title'])+(' →' if offset > 0 else '')+'</strong></a>'
        page_html = template.substitute(
            title=esc(page['title']), heading=esc(page['heading']), description=esc(page['description']),
            name=esc(config['name']), version=esc(config['version']), repository=esc(config['repository']),
            group=esc(page['group']), navigation=''.join(navigation), toc=page['toc'], content=page['body'],
            source=page['source'], previous=neighbor(-1, 'Previous'), next=neighbor(1, 'Next'),
            page_class='home' if page['slug']=='index' else 'reference')
        (output / (page['slug']+'.html')).write_text(page_html, encoding='utf-8')
    count = validate(output)
    print(f'Built {count} pages, {len(search)} searchable sections; all local links and anchors valid.')
    print(output / 'index.html')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build' / 'docs')
    args = parser.parse_args()
    try:
        build(args.output)
    except (ValueError, OSError) as exc:
        raise SystemExit(str(exc)) from exc


if __name__ == '__main__':
    main()
