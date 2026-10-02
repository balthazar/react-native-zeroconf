"""Builds the documentation website from docs/ into a single index.html.

Usage: python3 website/build.py docs website/dist/index.html
"""
import json
import os
import sys

WIKI = sys.argv[1]
OUT = sys.argv[2]

pages = {}
for name in sorted(os.listdir(WIKI)):
    if name.endswith('.md'):
        pages[name[:-3]] = open(os.path.join(WIKI, name), encoding='utf-8').read()

data = json.dumps(pages).replace('</', '<\\/')

html = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>react-native-zeroconf</title>
<meta name="description" content="Documentation for react-native-zeroconf: Bonjour / mDNS discovery and publishing for React Native on iOS, Android, macOS and tvOS.">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Ccircle cx='16' cy='16' r='14' fill='none' stroke='%230b7a68' stroke-width='2.5' opacity='.45'/%3E%3Ccircle cx='16' cy='16' r='8.5' fill='none' stroke='%230b7a68' stroke-width='2.5' opacity='.75'/%3E%3Ccircle cx='16' cy='16' r='3.5' fill='%230b7a68'/%3E%3C/svg%3E">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,600;12..96,700&family=Atkinson+Hyperlegible:ital,wght@0,400;0,700;1,400&family=JetBrains+Mono:wght@400;600&display=swap">
<style>
/* Layout: docs shell, service-record breadcrumb bar on top, sticky sidebar left, one readable column right */
:root {
  --bg: #f6f7f6;
  --surface: #ffffff;
  --fg: #1c2321;
  --muted: #5d6965;
  --line: #dfe4e2;
  --accent: #0b7a68;
  --accent-soft: #e2f1ed;
  --code-bg: #eef1f0;
  --warn: #9a5b00;
  --syn-keyword: #8a3ab9; --syn-string: #2d7a2d; --syn-comment: #7a8581; --syn-number: #b35c00; --syn-title: #1f5fa8; --syn-attr: #0b7a68;
  --font-display: "Bricolage Grotesque", "Avenir Next", "Segoe UI", sans-serif;
  --font-body: "Atkinson Hyperlegible", "Segoe UI", system-ui, sans-serif;
  --font-mono: "JetBrains Mono", ui-monospace, "SF Mono", Menlo, monospace;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #111615; --surface: #171e1c; --fg: #e3e9e7; --muted: #95a39e; --line: #2a3431;
    --accent: #4cc7ae; --accent-soft: #173029; --code-bg: #1d2623; --warn: #e0a84a;
    --syn-keyword: #d19bf0; --syn-string: #9fd88f; --syn-comment: #7f8d88; --syn-number: #f0b36b; --syn-title: #8cc4ff; --syn-attr: #4cc7ae;
    color-scheme: dark;
  }
}
:root[data-theme="dark"] {
  --bg: #111615; --surface: #171e1c; --fg: #e3e9e7; --muted: #95a39e; --line: #2a3431;
  --accent: #4cc7ae; --accent-soft: #173029; --code-bg: #1d2623; --warn: #e0a84a;
  --syn-keyword: #d19bf0; --syn-string: #9fd88f; --syn-comment: #7f8d88; --syn-number: #f0b36b; --syn-title: #8cc4ff; --syn-attr: #4cc7ae;
  color-scheme: dark;
}
* { box-sizing: border-box; }
html { color-scheme: light; }
body { margin: 0; background: var(--bg); color: var(--fg); font-family: var(--font-body); font-size: 16px; line-height: 1.6; }
a { color: var(--accent); text-decoration-thickness: 1px; text-underline-offset: 2px; }
a:focus-visible, button:focus-visible, summary:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; border-radius: 3px; }

.topbar {
  position: sticky; top: env(safe-area-inset-top, 0px); z-index: 2;
  display: flex; flex-wrap: wrap; align-items: baseline; gap: 4px 14px;
  padding: 12px 20px; background: var(--surface); border-bottom: 1px solid var(--line);
}
.brand { font-family: var(--font-display); font-weight: 700; font-size: 18px; letter-spacing: -0.01em; }
.record { font-family: var(--font-mono); font-size: 13px; color: var(--muted); min-width: 0; overflow-wrap: anywhere; }
.record b { color: var(--accent); font-weight: 600; }
.links { margin-left: auto; display: flex; gap: 14px; font-size: 14px; }
.links a { display: flex; color: var(--muted); padding: 4px; border-radius: 6px; }
.links a:hover { color: var(--accent); background: var(--accent-soft); }
.links svg { width: 20px; height: 20px; fill: currentColor; }

.shell { display: grid; grid-template-columns: 250px minmax(0, 1fr); gap: 40px; max-width: 1180px; margin: 0 auto; padding-block: 28px 64px; padding-inline: 20px; }
.sidebar { position: sticky; top: calc(env(safe-area-inset-top, 0px) + 72px); align-self: start; max-height: calc(100vh - 100px); overflow: auto; font-size: 15px; }
.sidebar details summary { display: none; }
.sidebar h1, .sidebar h2, .sidebar h3 { font-family: var(--font-display); font-size: 12px; text-transform: uppercase; letter-spacing: 0.08em; color: var(--muted); margin: 18px 0 6px; }
.sidebar ul { list-style: none; margin: 0; padding: 0; display: grid; gap: 2px; }
.sidebar ul ul { padding-left: 12px; }
.sidebar a { display: block; padding: 4px 10px; border-radius: 6px; color: var(--fg); text-decoration: none; }
.sidebar a:hover { background: var(--accent-soft); }
.sidebar a.current { background: var(--accent-soft); color: var(--accent); font-weight: 700; }
.sidebar p { margin: 6px 0; }

main { min-width: 0; }
.page { max-width: 72ch; }
.page h1 { font-family: var(--font-display); font-size: 40px; line-height: 1.1; letter-spacing: -0.02em; margin: 0 0 18px; text-wrap: balance; }
.page h2 { font-family: var(--font-display); font-size: 26px; line-height: 1.2; margin: 44px 0 12px; padding-top: 10px; border-top: 1px solid var(--line); text-wrap: balance; }
.page h3 { font-family: var(--font-display); font-size: 19px; margin: 30px 0 8px; text-wrap: balance; }
.page h4 { font-size: 16px; margin: 24px 0 6px; }
.page p, .page ul, .page ol { margin: 0 0 14px; }
.page li { margin: 4px 0; }
.page li > p { margin: 0; }
.page code { font-family: var(--font-mono); font-size: 0.88em; background: var(--code-bg); padding: 1px 5px; border-radius: 4px; }
.page pre { background: var(--code-bg); border: 1px solid var(--line); border-radius: 8px; padding: 14px 16px; overflow-x: auto; margin: 0 0 18px; line-height: 1.5; }
.page pre code { background: none; padding: 0; font-size: 13.5px; }
.page blockquote { margin: 0 0 16px; padding: 10px 16px; border-left: 3px solid var(--accent); background: var(--accent-soft); border-radius: 0 8px 8px 0; }
.page blockquote p:last-child { margin-bottom: 0; }
.table-wrap { overflow-x: auto; margin: 0 0 18px; border: 1px solid var(--line); border-radius: 8px; }
.page table { border-collapse: collapse; width: 100%; font-size: 14.5px; }
.page th, .page td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); vertical-align: top; }
.page th[align=center], .page td[align=center] { text-align: center; }
.page th { background: var(--surface); font-weight: 700; }
.page tr:last-child td { border-bottom: none; }
.page hr { border: none; border-top: 1px solid var(--line); margin: 28px 0; }
.page h1 a.anchor, .page h2 a.anchor, .page h3 a.anchor { visibility: hidden; margin-left: 8px; color: var(--muted); text-decoration: none; font-weight: 400; }
.page h1:hover a.anchor, .page h2:hover a.anchor, .page h3:hover a.anchor { visibility: visible; }
.hljs-keyword, .hljs-built_in, .hljs-literal, .hljs-meta .hljs-keyword { color: var(--syn-keyword); }
.hljs-string, .hljs-regexp, .hljs-template-string { color: var(--syn-string); }
.hljs-comment, .hljs-quote { color: var(--syn-comment); font-style: italic; }
.hljs-number { color: var(--syn-number); }
.hljs-title, .hljs-title.function_, .hljs-title.class_ { color: var(--syn-title); }
.hljs-attr, .hljs-attribute, .hljs-name, .hljs-tag, .hljs-property { color: var(--syn-attr); }
.hljs-variable, .hljs-params { color: var(--fg); }
.page [data-tip] { cursor: help; border-bottom: 1px dotted var(--muted); outline: none; }
.tip-pop { position: fixed; z-index: 50; max-width: 260px; padding: 6px 10px; border-radius: 6px; background: var(--fg); color: var(--bg); font-size: 13px; line-height: 1.4; pointer-events: none; opacity: 0; transition: opacity 0.12s; }
.tip-pop.visible { opacity: 1; }
.footer { margin-top: 48px; padding-top: 16px; border-top: 1px solid var(--line); color: var(--muted); font-size: 14px; }
.missing { color: var(--warn); }

@media (max-width: 820px) {
  .shell { grid-template-columns: minmax(0, 1fr); gap: 16px; padding-block: 16px 48px; padding-inline: 16px; }
  .sidebar { position: static; max-height: none; }
  .sidebar details { border: 1px solid var(--line); border-radius: 8px; background: var(--surface); padding: 6px 10px; }
  .sidebar details summary { display: block; cursor: pointer; font-weight: 700; padding: 4px 0; }
  .page h1 { font-size: 32px; }
  .links { margin-left: 0; }
}
@media (prefers-reduced-motion: no-preference) { html { scroll-behavior: smooth; } }
</style>

</head>
<body>
<header class="topbar">
  <a class="brand" href="/" style="color: inherit; text-decoration: none">react-native-zeroconf</a>
  <span class="record" id="record">_docs._tcp.local.</span>
  <nav class="links" aria-label="Project">
    <!-- Logos from Simple Icons (CC0) -->
    <a href="https://github.com/balthazar/react-native-zeroconf" aria-label="GitHub repository" title="GitHub"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12"/></svg></a>
    <a href="https://www.npmjs.com/package/react-native-zeroconf" aria-label="npm package" title="npm"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M1.763 0C.786 0 0 .786 0 1.763v20.474C0 23.214.786 24 1.763 24h20.474c.977 0 1.763-.786 1.763-1.763V1.763C24 .786 23.214 0 22.237 0zM5.13 5.323l13.837.019-.009 13.836h-3.464l.01-10.382h-3.456L12.04 19.17H5.113z"/></svg></a>
  </nav>
</header>
<div class="shell">
  <nav class="sidebar" aria-label="Wiki pages">
    <details id="nav-details" open>
      <summary>Pages</summary>
      <div id="sidebar"></div>
    </details>
  </nav>
  <main>
    <article class="page" id="page"></article>
    <div class="footer" id="footer"></div>
  </main>
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/marked/15.0.7/marked.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.10.0/highlight.min.js"></script>
<script type="application/json" id="wiki-data">''' + data + r'''</script>
<script>
(function () {
  var pages = JSON.parse(document.getElementById('wiki-data').textContent);
  var pageEl = document.getElementById('page');
  var sidebarEl = document.getElementById('sidebar');
  var footerEl = document.getElementById('footer');
  var recordEl = document.getElementById('record');
  var navDetails = document.getElementById('nav-details');

  function pageKey(name) {
    var key = decodeURIComponent(name).trim().replace(/\s+/g, '-');
    if (pages[key]) return key;
    var lower = key.toLowerCase();
    for (var k in pages) { if (k.toLowerCase() === lower) return k; }
    return key;
  }

  // GitHub-style heading anchors
  function slug(text) {
    return text.trim().toLowerCase().replace(/[^\w\- ]+/g, '').replace(/ /g, '-');
  }

  // [[Page]] and [[Text|Page]] wiki links
  function wikiLinks(md) {
    return md.replace(/\[\[([^\]|]+)(?:\|([^\]]+))?\]\]/g, function (_, a, b) {
      var text = a, target = b || a;
      return '[' + text + '](' + target.trim().replace(/\s+/g, '-') + ')';
    });
  }

  function render(md) {
    return marked.parse(wikiLinks(md), { gfm: true });
  }

  function decorate(root) {
    var seen = {};
    root.querySelectorAll('h1, h2, h3, h4').forEach(function (h) {
      var id = slug(h.textContent);
      if (seen[id] !== undefined) { seen[id] += 1; id = id + '-' + seen[id]; } else { seen[id] = 0; }
      h.id = id;
      if (h.tagName !== 'H4') {
        var a = document.createElement('a');
        a.className = 'anchor'; a.href = '#'; a.textContent = '#'; a.setAttribute('aria-label', 'Link to this section');
        a.dataset.anchor = id;
        h.appendChild(a);
      }
    });
    if (window.hljs) {
      root.querySelectorAll('pre code').forEach(function (code) {
        var match = (code.className.match(/language-(\w+)/) || [])[1];
        if (match === 'tsx' || match === 'jsx') { code.className = 'language-typescript'; match = 'typescript'; }
        if (match && hljs.getLanguage(match)) hljs.highlightElement(code);
      });
    }
    root.querySelectorAll('table').forEach(function (t) {
      var wrap = document.createElement('div');
      wrap.className = 'table-wrap';
      t.parentNode.insertBefore(wrap, t);
      wrap.appendChild(t);
    });
    // Notes on table cells: the title becomes a tooltip shown on hover, focus or tap
    root.querySelectorAll('td span[title]').forEach(function (span) {
      span.setAttribute('data-tip', span.title);
      span.removeAttribute('title');
      span.tabIndex = 0;
    });
  }

  var tip = document.createElement('div');
  tip.className = 'tip-pop';
  tip.setAttribute('role', 'tooltip');
  document.body.appendChild(tip);
  function showTip(el) {
    tip.textContent = el.getAttribute('data-tip');
    tip.classList.add('visible');
    var r = el.getBoundingClientRect();
    var left = Math.max(8, Math.min(r.left + r.width / 2 - tip.offsetWidth / 2, window.innerWidth - tip.offsetWidth - 8));
    var top = r.top - tip.offsetHeight - 6;
    if (top < 8) top = r.bottom + 6;
    tip.style.left = left + 'px';
    tip.style.top = top + 'px';
  }
  function hideTip() { tip.classList.remove('visible'); }
  document.addEventListener('mouseover', function (e) {
    var el = e.target.closest && e.target.closest('[data-tip]');
    if (el) showTip(el); else hideTip();
  });
  document.addEventListener('focusin', function (e) {
    if (e.target.hasAttribute && e.target.hasAttribute('data-tip')) showTip(e.target);
  });
  document.addEventListener('focusout', hideTip);
  window.addEventListener('scroll', hideTip, true);

  var current = null;

  function show(name, anchor) {
    var key = pageKey(name || 'Home');
    if (!pages[key]) {
      pageEl.innerHTML = '<h1>Page not found</h1><p class="missing">There is no page named <code>' + key + '</code> in this draft.</p>';
    } else if (key !== current) {
      pageEl.innerHTML = render(pages[key]);
      decorate(pageEl);
    }
    current = key;
    recordEl.innerHTML = '<b>' + key + '</b>._docs._tcp.local.';
    document.title = key === 'Home' ? 'react-native-zeroconf' : key.replace(/-/g, ' ') + ' · react-native-zeroconf';
    // Highlight one sidebar link: the one for this section if there is one, else the page link
    var links = Array.prototype.slice.call(sidebarEl.querySelectorAll('a'));
    var match = links.filter(function (a) { return a.dataset.page === key && anchor && a.dataset.section === anchor; })[0]
      || links.filter(function (a) { return a.dataset.page === key && !a.dataset.section; })[0];
    links.forEach(function (a) { a.classList.toggle('current', a === match); });
    if (anchor) {
      var target = document.getElementById(anchor);
      if (target) { target.scrollIntoView(); return; }
    }
    window.scrollTo(0, 0);
  }

  function internalTarget(href) {
    if (!href || /^[a-z][a-z0-9+.-]*:/i.test(href) || href.indexOf('//') === 0) return null;
    var hashIndex = href.indexOf('#');
    var name = hashIndex === -1 ? href : href.slice(0, hashIndex);
    var anchor = hashIndex === -1 ? '' : href.slice(hashIndex + 1);
    name = name.replace(/^\.?\//, '').replace(/\.md$/, '').replace(/^wiki\//, '');
    if (name === '') name = hashIndex === -1 ? 'Home' : '';
    return { name: name, anchor: anchor };
  }

  function wireLinks(root) {
    root.addEventListener('click', function (event) {
      var a = event.target.closest('a');
      if (!a) return;
      if (a.dataset.anchor) {
        event.preventDefault();
        var el = document.getElementById(a.dataset.anchor);
        if (el) el.scrollIntoView();
        return;
      }
      var target = internalTarget(a.getAttribute('href'));
      if (!target) return;
      event.preventDefault();
      if (!target.name) {
        var el2 = document.getElementById(target.anchor);
        if (el2) el2.scrollIntoView();
        return;
      }
      var key = pageKey(target.name);
      var url = (key === 'Home' ? '/' : '/' + key) + (target.anchor ? '#' + target.anchor : '');
      try { history.pushState(null, '', url); } catch (e) {}
      show(key, target.anchor);
      if (window.matchMedia('(max-width: 820px)').matches) navDetails.open = false;
    });
  }

  // Sidebar and footer
  if (pages._Sidebar) {
    sidebarEl.innerHTML = render(pages._Sidebar);
    sidebarEl.querySelectorAll('a').forEach(function (a) {
      var t = internalTarget(a.getAttribute('href'));
      if (t && t.name) { a.dataset.page = pageKey(t.name); a.dataset.section = t.anchor || ''; }
    });
  } else {
    sidebarEl.innerHTML = '<ul>' + Object.keys(pages).filter(function (k) { return k[0] !== '_'; }).map(function (k) {
      return '<li><a href="' + k + '" data-page="' + k + '">' + k.replace(/-/g, ' ') + '</a></li>';
    }).join('') + '</ul>';
  }
  if (pages._Footer) footerEl.innerHTML = render(pages._Footer);

  wireLinks(pageEl);
  wireLinks(sidebarEl);
  wireLinks(footerEl);

  function fromLocation() {
    var path = decodeURIComponent(location.pathname.replace(/^\/+|\/+$/g, ''));
    var anchor = (location.hash || '').replace(/^#/, '');
    show(path || 'Home', anchor);
  }
  window.addEventListener('popstate', fromLocation);

  if (window.matchMedia('(max-width: 820px)').matches) navDetails.open = false;
  fromLocation();
})();
</script>
</body>
</html>
'''

os.makedirs(os.path.dirname(OUT) or '.', exist_ok=True)
open(OUT, 'w', encoding='utf-8').write(html)
print(f'{len(pages)} pages, {len(html)} bytes -> {OUT}')
