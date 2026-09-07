#!/usr/bin/env python3
"""Tarkista upstream erillisessä tilapäisessä kloonissa. Ei kirjoituksia lähderepoon."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def run(args, cwd, **kwargs):
    return subprocess.run(args, cwd=cwd, check=True, **kwargs)


def output(args, cwd):
    return run(args, cwd, stdout=subprocess.PIPE, text=True).stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2], help='Tarkistettava puhdas Git-repo')
    parser.add_argument('--upstream-url', default='https://github.com/Mikky100/ASkompu.git', help='Upstream-repo; paikallinen polku sopii työkalun testaukseen')
    parser.add_argument('--merge-only', action='store_true', help='Tarkista vain yhdistettävyys, älä rakenna tai testaa')
    parser.add_argument('--firmware', action='store_true', help='Rakenna lisäksi molemmat firmware-kohteet (pio tarvitaan)')
    args = parser.parse_args()
    source = args.source.resolve()
    if output(['git', 'status', '--porcelain', '--untracked-files=normal'], source):
        print('Virhe: lähderepo ei ole puhdas. Tallenna työsi ensin; tarkistus käyttää vain HEAD-committia.', file=sys.stderr)
        return 2
    revision = output(['git', 'rev-parse', 'HEAD'], source)
    with tempfile.TemporaryDirectory(prefix='askompu-upstream-') as temp:
        clone = Path(temp) / 'repo'
        run(['git', 'clone', '--no-local', '--no-hardlinks', '--no-checkout', str(source), str(clone)], source)
        run(['git', 'checkout', '--detach', revision], clone)
        run(['git', 'remote', 'add', 'upstream', args.upstream_url], clone)
        run(['git', 'fetch', '--no-tags', 'upstream', 'main'], clone)
        upstream = output(['git', 'rev-parse', 'upstream/main'], clone)
        print(f'Tarkistetaan simulaattori {revision} + upstream {upstream}', flush=True)
        try:
            run(['git', '-c', 'user.name=ASkompu-yhteensopivuustarkistus',
                 '-c', 'user.email=compatibility@example.invalid',
                 'merge', '--no-commit', '--no-ff', 'upstream/main'], clone)
        except subprocess.CalledProcessError:
            conflicts = output(['git', 'diff', '--name-only', '--diff-filter=U'], clone)
            print(f'Virhe: tilapäinen upstream-yhdistäminen epäonnistui. Ristiriidat:\n{conflicts or "Katso Git-tuloste."}', file=sys.stderr)
            return 3
        if not args.merge_only:
            simulator = clone / 'simulator'
            run(['cmake', '--preset', 'linux-release'], simulator)
            run(['cmake', '--build', '--preset', 'linux-release'], simulator)
            run(['ctest', '--preset', 'linux-release'], simulator)
            if args.firmware:
                for target in ('lilygo-main', 'esp32s3-ili9488-main'):
                    run(['pio', 'run', '-e', target], clone)
        print('Hyväksytty: vain tilapäinen klooni muuttui. Mitään ei commitattu tai pushattu.')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, OSError) as error:
        print(f'Virhe: yhteensopivuustarkistus keskeytyi: {error}', file=sys.stderr)
        sys.exit(1)
