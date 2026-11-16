(() => {
  'use strict';
  const byId = id => document.getElementById(id);
  const menu = byId('menuToggle');
  function closeMenu() {
    document.body.classList.remove('nav-open');
    menu.setAttribute('aria-expanded', 'false');
  }
  menu.addEventListener('click', () => {
    const open = document.body.classList.toggle('nav-open');
    menu.setAttribute('aria-expanded', String(open));
  });
  document.addEventListener('keydown', event => { if (event.key === 'Escape') closeMenu(); });
  document.querySelectorAll('#navigation a').forEach(link => link.addEventListener('click', closeMenu));
  const current = document.querySelector('#navigation [aria-current]');
  if (current) {
    const sidebar = byId('navigation');
    sidebar.scrollTop = Math.max(0, current.offsetTop - sidebar.clientHeight / 2);
  }
  document.querySelectorAll('.prose pre').forEach(pre => {
    const code = pre.querySelector('code');
    if (!code) return;
    const button = document.createElement('button');
    button.type = 'button'; button.className = 'copy-code'; button.textContent = 'Copy';
    button.setAttribute('aria-label', 'Copy code to clipboard');
    button.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(code.textContent);
        button.textContent = 'Copied';
      } catch (_) { button.textContent = 'Select text to copy'; }
      setTimeout(() => { button.textContent = 'Copy'; }, 2000);
    });
    pre.prepend(button);
  });
  const dialog = byId('searchDialog'), input = byId('searchInput');
  const results = byId('searchResults'), count = byId('searchCount');
  const records = window.FANMASTER_SEARCH || [];
  function openSearch() { if (!dialog.open) dialog.showModal(); input.focus(); }
  byId('searchOpen').addEventListener('click', openSearch);
  byId('searchClose').addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', event => { if (event.target === dialog) dialog.close(); });
  document.addEventListener('keydown', event => {
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault(); openSearch();
    }
  });
  input.addEventListener('input', () => {
    const query = input.value.trim().toLocaleLowerCase().slice(0, 160);
    results.replaceChildren();
    if (!query) { count.textContent = 'Search pages and sections. Everything stays in your browser.'; return; }
    const terms = query.split(/\s+/);
    const matches = records.map(record => {
      const title = `${record.title} ${record.section}`.toLocaleLowerCase();
      const text = record.text.toLocaleLowerCase();
      const score = terms.every(term => `${title} ${text}`.includes(term))
        ? terms.reduce((n, term) => n + (title.includes(term) ? 10 : 1), 0) : 0;
      return { record, score };
    }).filter(item => item.score).sort((a, b) => b.score - a.score);
    count.textContent = matches.length ? `${matches.length} matching sections · showing up to 12` : 'No results. Try another term.';
    for (const { record } of matches.slice(0, 12)) {
      const item = document.createElement('li'), link = document.createElement('a');
      link.href = record.url;
      const page = document.createElement('span'); page.textContent = record.title;
      const heading = document.createElement('strong'); heading.textContent = record.section;
      const excerpt = document.createElement('p');
      const first = Math.max(0, record.text.toLocaleLowerCase().indexOf(terms[0]) - 45);
      excerpt.textContent = (first ? '…' : '') + record.text.slice(first, first + 160) + (record.text.length > first + 160 ? '…' : '');
      link.append(page, heading, excerpt); item.append(link); results.append(item);
    }
  });
  if ('IntersectionObserver' in window) {
    const links = [...document.querySelectorAll('.on-this-page .toc a')];
    const observer = new IntersectionObserver(entries => {
      for (const entry of entries) {
        if (!entry.isIntersecting) continue;
        links.forEach(link => {
          if (decodeURIComponent(link.hash.slice(1)) === entry.target.id) link.setAttribute('aria-current', 'location');
          else link.removeAttribute('aria-current');
        });
      }
    }, { rootMargin: '-90px 0px -65% 0px' });
    document.querySelectorAll('.prose h2[id], .prose h3[id]').forEach(heading => observer.observe(heading));
  }
})();
