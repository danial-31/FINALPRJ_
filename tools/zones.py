"""Zone source of truth for THE OSSUARY: Ashvald.

Run `python tools/zones.py` after editing a map. It validates every map (row widths,
legend, reachability of exits, NPCs and objects) and regenerates src/zone_data.inc.
Python is a development tool only; the build uses the generated file.
"""
from collections import deque
from pathlib import Path

# Terrain shared by every zone. Anything else in a map must be an anchor from the zone legend.
TERRAIN = {
    '#': 'Wall', '.': 'Floor', ',': 'Path', 'T': 'Prop', 'H': 'Prop', 'K': 'Wall', '~': 'Water',
    '=': 'Shallow', 'w': 'Web', 'p': 'Poison', '^': 'Trap', 'r': 'Prop', 'c': 'LowProp',
    'x': 'LowProp', 'f': 'LowProp', 's': 'Prop', 'k': 'Decor', 'g': 'LowProp', 'l': 'Prop',
    'P': 'Prop', 'b': 'Decor', 'F': 'Decor', 'W': 'Prop', 'B': 'Prop', 'm': 'Prop', 'n': 'Prop',
    'y': 'Prop', 'd': 'Decor', 'u': 'Decor', 'v': 'Prop',
}
BLOCKING_ENTITIES = {'Npc', 'Shrine', 'Chest', 'Lever', 'Gate', 'Cocoon', 'Ore', 'Note', 'Survivor',
                     'Rune', 'Cargo', 'Clue', 'KeyCache', 'RuneDoor', 'LockDoor'}

def E(kind, ident='', arg='', cond='', label='', msg=''):
    return dict(type=kind, id=ident, arg=arg, cond=cond, label=label, msg=msg)

def S(kind, group=0):
    return dict(spawn=kind, group=group)

ZONES = []

def fit(r, w):
    """Authoring aid: widen with floor before the right wall, or trim the longest wall run."""
    while len(r) < w:
        i = max(r.rfind('.'), r.rfind(','))
        r = r[:i] + r[i] + r[i:]
    while len(r) > w:
        best, start, i = (0, 0), 0, 0
        while i < len(r):
            j = i
            while j < len(r) and r[j] == r[i]:
                j += 1
            if r[i] in '#KT' and j - i > best[0]:
                best = (j - i, i)
            i = j
        r = r[:best[1]] + r[best[1] + 1:]
    return r

def zone(ident, name, biome, surface, music, region, rows, legend):
    if rows:
        rows = [fit(r, len(rows[0])) for r in rows]
    ZONES.append(dict(id=ident, name=name, biome=biome, surface=surface, music=music,
                      region=region, rows=rows, legend=legend))

# ---------------------------------------------------------------- Grey Ford village
zone('village', 'Серый Брод', 0, True, 'village', 'village', [
    'T'*17 + 'N' + 'T'*18,
    'T'*16 + '.,.' + 'T'*9 + '~~' + 'T'*6,
    'TTT.bb..' + 'T'*5 + '....)....' + 'T'*5 + '.~~.' + 'T'*5,
    'TT..HHHH..TT.....,......TT..~~..TTTT',
    'T...HHHH.........,..........~~....TT',
    'T...HHHH..E......,.....HHH..~~.HHH.T',
    'T.....b...,,,,,,,,.....HHH..~~.HHH.T',
    'T.HHH....,.......,.....HHH..~~.HHHGT',
    'T.HHH...,........,..........~~.....T',
    'T.HHHA..,....F...,....M.....~~.....T',
    'T.xx....,......W.,..........~~.....T',
    'T.......' + ','*20 + '==' + ',,,},(',
    'T.......,...S...............~~.....T',
    'T...b...,...........HHH.....~~.....T',
    'T.......,....U......HHH.....~~.....T',
    'T.HHH...,...........HHH.....~~...TTT',
    'T.HHH...,...................~~..TTTT',
    'T.HHH...,..ff.ff............~~TTTTTT',
    'T.......,..fF.Ff............~~TTTTTT',
    'TT......@..ff.ff........TT..~~TTTTTT',
    'TTT.....,..............TTTT.~~TTTTTT',
    'TTTT...,,..........TTTTTTT..~~TTTTTT',
    'TTTTT.,,..' + 'T'*18 + '~~' + 'T'*6,
    'T'*6 + ',' + 'T'*21 + '~~' + 'T'*6,
], {
    'N': E('Exit', 'to_castle', 'castle@(', 'seals>=3', 'Дорога к замку', 'Ворота замка запечатаны скверной. Нужны три печати.'),
    ')': E('Arrive', 'from_castle'), '}': E('Arrive', 'from_road'), '@': E('Arrive', 'start'),
    '(': E('Exit', 'to_road', 'road@['),
    'E': E('Npc', 'elder', label='Староста Ольвин'),
    'A': E('Npc', 'smith', label='Кузнец Борн'),
    'M': E('Npc', 'merchant', label='Торговка Мира'),
    'G': E('Npc', 'herbalist', label='Травница Ива'),
    'U': E('Npc', 'survivor', cond='flag:rescued_miner', label='Шахтёр Тоби'),
    'S': E('Shrine', 'shrine_village', label='Святилище Серого Брода'),
})

# ---------------------------------------------------------------- Old road
zone('road', 'Старая дорога', 1, True, 'explore', 'road', [
    'T'*19 + 'N' + 'T'*8 + '#'*10 + 'X#',
    'T'*18 + '.].' + 'T'*6 + '#'*11 + '}#',
    'T'*17 + '..,..' + 'TTTT' + '#'*12 + 'R#',
    'T'*15 + '...b,...TTTrr' + '#'*10 + ',#',
    'TTTTTT.....TTTT....,....TT...r' + '#'*8 + ',#',
    'TTTT..P..P..TTT1...,.....T.....rr#####,#',
    'TTT...!.....TT.....,....1......r.....,,#',
    'TT..P....P.........,..........' + '.'*7 + ',r#',
    'T.........' + '....c....' + ',' + ','*17 + ',rr',
    'T....c......x......,.........TT.......rr',
    'T' + '.'*18 + ',' + '.'*11 + 'T......rr',
    '<[' + ','*36 + '{E',
    'T' + ','*12 + 'c' + ','*14 + 'c' + ','*10 + 'T',
    'T.....x......c..........2.....k........T',
    'TT' + '.'*10 + 'TT' + '.'*11 + 'nn....nn.....TT',
    'TTTrrr....TTTT' + '.'*9 + '3...l....2.....TT',
    'TTr$r.....TTT' + '.'*10 + 'x....Q...x.....TT',
    'TT' + '.'*9 + 'TT' + '.'*8 + 'nn' + '.'*5 + '3' + '.'*7 + 'TTTT',
    'TTT' + '.'*9 + 'T' + '.'*10 + 'TT' + '.'*8 + 'T'*7,
    'TTTT......TTTTT.......TTTT......' + 'T'*8,
    'T'*6 + '....' + 'T'*8 + '...' + 'T'*19,
    'T'*40, 'T'*40, 'T'*40,
], {
    '<': E('Exit', 'to_village', 'village@}'), '[': E('Arrive', 'from_village'),
    'E': E('Exit', 'to_mine', 'mine@['), '{': E('Arrive', 'from_mine'),
    'N': E('Exit', 'to_grove', 'grove@['), ']': E('Arrive', 'from_grove'),
    'X': E('Exit', 'to_pass', 'pass@['), '}': E('Arrive', 'from_pass'),
    'R': E('Gate', 'rockslide', cond='item:climbing_gear', label='Обвал', msg='Обвал перекрыл тропу. Нужно снаряжение для подъёма.'),
    '!': E('Note', 'guard_journal', label='Дневник стража'),
    '$': E('Chest', 'road_stash', 'gold:45;potion:1;ore:2', label='Тайник под камнями'),
    'Q': E('Cargo', 'smith_cargo', label='Ящик с клеймом кузни'),
    '1': S('Wolf', 1), '2': S('Bandit', 2), '3': S('BanditArcher', 2),
})

# ---------------------------------------------------------------- Spider grove
zone('grove', 'Паучья роща', 2, True, 'explore', 'grove', [
    'T'*34,
    'T'*12 + '#'*10 + 'T'*12,
    'T'*10 + '######L#####TT' + 'T'*10,
    'T'*8 + 'yw#....k(....#wy' + 'T'*10,
    'T'*6 + 'www....S.....?..wwy' + 'T'*9,
    'TTTTyw..2.....Fu.....Cw..TTTT%TTTT',
    'TTT.......TT..u........1.........T',
    'TTCw....TTT......yy......wC.....TT',
    'TT...1..T...ww....TT...2...TTTTT.T',
    'TTT....TT....w....J.....TT......TT',
    'TTTT%.....y........' + '...w....1....TT',
    'TTT.........,,,...TTT.........TTTT',
    'TTy...........,..w...........yTTTT',
    'TTTCw.........,........TT.....TTTT',
    'TTTT..TT......,...2.......wC..TTTT',
    'TTTTT...TT...,,.......TT....TTTTTT',
    'TTTTTT%.....,...b......TT..TTTTTTT',
    'TTTTTTT.....,......TTT...TTTTTTTTT',
    'TTTTTTTT....,....' + 'T'*17,
    'T'*10 + '..[..' + 'T'*19,
    'T'*11 + '.,.' + 'T'*20,
    'T'*12 + 'D' + 'T'*21,
    'T'*34, 'T'*34,
], {
    'L': E('Entrance', 'to_lair', 'lair@@', label='Логово пауков'), '(': E('Arrive', 'from_lair'),
    'D': E('Exit', 'to_road', 'road@]'), '[': E('Arrive', 'from_road'),
    'S': E('Shrine', 'shrine_grove', 'lair@@', label='Святилище рощи'),
    '?': E('Clue', 'miner_traces', label='Брошенная шахтёрская каска'),
    'J': E('KeyCache', 'guard_key', label='Дупло старого дуба'),
    'C': E('Cocoon', 'grove_cocoon', 'silk:1;herb:1', label='Кокон'),
    '%': E('Herb', 'herb', label='Целебная трава'),
    '1': S('Spiderling', 0), '2': S('VenomSpider', 0),
})

# ---------------------------------------------------------------- Spider lair (dungeon 1)
zone('lair', 'Паучье логово', 10, False, 'dungeon', 'grove', [
    '#'*34,
    '#'*9 + 'w..k........k..w' + '#'*9,
    '#'*8 + '.w..............w.' + '#'*8,
    '#'*7 + '..s......9......s..' + '#'*8,
    '#'*7 + '...k...........k...' + '#'*8,
    '#'*7 + '..w.............w..' + 'Y' + '#'*7,
    '#'*8 + '.'*17 + '#'*9,
    '#'*10 + '.'*13 + '#'*11,
    '#'*15 + '...' + '#'*16,
    '#'*15 + 'w.w' + '#'*16,
    '###......######...####.......#####',
    '###.C..!.######..w####..Q...$#####',
    '###......ww.....2.........p..#####',
    '###..p...######...####.1..p..#####',
    '###.p.1..######w..####C....U.#####',
    '###......######...####.......#####',
    '#'*15 + '...' + '#'*16,
    '#'*10 + '..3.....' + '..p..' + '#'*11,
    '#'*8 + '.w.....p.p.....w.' + '#'*9,
    '#'*8 + '..Q.....2.....C..' + '#'*9,
    '#'*8 + '.....ww....ww....' + '#'*9,
    '#'*10 + '.....1.......' + '#'*11,
    '#'*13 + '.......' + '#'*14,
    '#'*14 + '.@...' + '#'*15,
    '#'*16 + 'D' + '#'*17,
    '#'*34,
], {
    'D': E('Exit', 'to_grove', 'grove@('), '@': E('Arrive', 'entry'),
    'Y': E('Exit', 'boss_exit', 'grove@(', 'flag:boss_matriarch', 'Боковой тоннель', 'Тоннель затянут паутиной Матки.'),
    'C': E('Cocoon', 'lair_cocoon', 'silk:1;venom:1;gold:12', label='Кокон'),
    'Q': E('Cocoon', 'lair_ambush', 'enemy:Spiderling', label='Шевелящийся кокон'),
    'U': E('Survivor', 'miner_survivor', label='Опутанный шахтёр'),
    '!': E('Note', 'lair_note', label='Записка шахтёра'),
    '$': E('Chest', 'lair_cache', 'gold:60;greater_potion:1;venom:2', label='Тайник в паутине'),
    '1': S('Spiderling', 0), '2': S('VenomSpider', 0), '3': S('SpiderGuard', 0), '9': S('Matriarch', 9),
})

# ---------------------------------------------------------------- Mine yard (surface)
zone('mine', 'Заброшенная шахта', 3, True, 'explore', 'mine', [
    '#'*34,
    '#'*12 + '###L####' + '#'*14,
    '##########rr..(..rr######' + '#'*9,
    '#########O.......m..v####' + '#'*9,
    '########..m...........###' + '#'*9,
    '######....x....S.......##' + '##TT###',
    '#####...cc.............O.' + '.TTTT##',
    '####.....,,,,,,,,,,,...1.' + '.....##',
    '###..1...,.......HH.,....' + '..2..##',
    '###......,..?....HH.,....' + '.....##',
    '##.......,.......x..,..m.' + '....###',
    '##.%.....,..........,....' + '...####',
    '#[,,,,,,,,..........,,,,,' + ',.O####',
    'D,.......d....m.....d....' + '...####',
    '##....1........3.........' + '..#####',
    '##.......x.....' + '....' + '.....!' + '.######',
    '###....' + '....' + 'rr.....$..rr' + '.' + '#'*7,
    '####...' + 'TT' + '.......' + 'TTTTTTT' + '#'*9,
    '#####' + 'T'*7 + '....' + 'T'*18,
    'T'*34, 'T'*34, 'T'*34, 'T'*34, 'T'*34,
], {
    'L': E('Entrance', 'to_flooded', 'flooded@@', 'stage:main>=5', 'Ствол шахты', 'Вход заколочен. Староста знает, как его открыть.'),
    '(': E('Arrive', 'from_flooded'),
    'D': E('Exit', 'to_road', 'road@{'), '[': E('Arrive', 'from_road'),
    'S': E('Shrine', 'shrine_mine', 'flooded@@', label='Святилище рудокопов'),
    '?': E('Clue', 'mine_traces', label='Следы волочения к шахте'),
    'O': E('Ore', 'ore', 'ore:2', label='Рудная жила'),
    '%': E('Herb', 'herb', label='Целебная трава'),
    '!': E('Note', 'foreman_note', label='Записка бригадира'),
    '$': E('Chest', 'mine_crate', 'gold:35;potion:1;ore:2', label='Ящик бригадира'),
    '1': S('Rat', 0), '2': S('Slime', 0), '3': S('Miner', 0),
})

# ---------------------------------------------------------------- Flooded mines (dungeon 2)
zone('flooded', 'Затопленные шахты', 11, False, 'dungeon', 'mine', [
    '#'*36,
    '#'*23 + 'm..........' + '##',
    '###.....' + '#'*15 + '...........' + '##',
    '###.O.^.' + '#'*15 + '.....9.....' + '##',
    '###.....###.........###...........##',
    '###..x..###.&..m....###..........Y##',
    '###..2.....' + '.........' + '###...........##',
    '###.....###...3.....###...........##',
    '###.....###.........' + '#'*8 + 'G' + '#'*7,
    '###~~=~~' + '#'*7 + '...' + '#'*10 + '.' + '#'*7,
    '###.....' + '#'*7 + '.^.' + '#'*10 + '.' + '#'*7,
    '###..1.....x......========....' + '..####',
    '###.......m.....2...~~==~~..' + '...O####',
    '###.....' + '#'*7 + '...' + '#'*6 + '.....' + '...####',
    '###..O..' + '#'*7 + '.^.' + '#'*6 + '.4...' + '..*####',
    '###.....' + '#'*7 + '...' + '#'*6 + '.....' + '...####',
    '###.....' + '#'*7 + '...' + '#'*6 + '~~~~=~~~' + '####',
    '###.1.....x..x.....3.....' + '...!..' + '####',
    '###..^....m.......' + '......' + '..$.....' + '####',
    '###.....' + '#'*7 + '...' + '#'*18,
    '###..@..' + '#'*7 + '...' + '#'*18,
    '###..D..' + '#'*7 + '.4.' + '#'*18,
    '#'*15 + '.O.' + '#'*18,
    '#'*36,
], {
    'D': E('Exit', 'to_mine', 'mine@('), '@': E('Arrive', 'entry'),
    'Y': E('Exit', 'boss_exit', 'mine@(', 'flag:boss_warden', 'Подъёмник', 'Подъёмник обесточен.'),
    '&': E('Lever', 'lever_a', label='Насос западной штольни'),
    '*': E('Lever', 'lever_b', label='Насос восточной штольни'),
    'G': E('Gate', 'pump_gate', cond='flag:lever_a,flag:lever_b', label='Затвор', msg='Затвор открывается, только когда работают оба насоса.'),
    'O': E('Ore', 'ore', 'ore:2', label='Рудная жила'),
    '!': E('Note', 'mage_note_1', label='Обгоревший лист'),
    '$': E('Chest', 'flooded_cache', 'gold:70;greater_potion:1;bone:2;ore:2', label='Сундук смотрителя'),
    '1': S('Rat', 0), '2': S('Slime', 0), '3': S('Miner', 0), '4': S('Golem', 0), '9': S('Warden', 9),
})

# ---------------------------------------------------------------- Mountain pass
zone('pass', 'Горный перевал', 4, True, 'explore', 'pass', [
    '#'*40,
    '#'*25 + '##L###' + '#'*9,
    '#'*8 + '..rP..P' + '#'*9 + '...(....' + '#'*8,
    '#'*7 + '.P..$..P' + '#'*7 + '...S......3..' + '#'*5,
    '#'*7 + '..r...r' + '#'*7 + '...............' + '#'*4,
    '#'*8 + '.......' + '#'*6 + '....4.......r....' + '#'*3,
    '#'*10 + '..%' + '.' + '#'*5 + '......' + '#'*7 + '......' + '#'*3,
    '#'*11 + '.,..' + '#'*3 + '.....' + '#'*10 + '%....' + '#'*3,
    '#'*12 + ',..' + '...O.' + '#'*12 + '.....' + '#'*3,
    '#'*12 + ',....' + '1..' + '#'*12 + '..2..' + '#'*3,
    '#'*11 + '..,..' + '#'*2 + '...' + '#'*11 + '.....' + '#'*3,
    '#'*10 + '...,.' + '#'*4 + '....' + '#'*8 + '.......' + '#'*3,
    '#'*9 + '...,..' + '#'*5 + '......2' + '.' + '......' + '#'*5,
    '#'*8 + '...,...' + '#'*6 + '..r.........' + '#'*7,
    '#'*7 + '...,....' + '#'*7 + '...1.....' + '#'*9,
    '#'*6 + '..,,...O' + '#'*9 + '....' + '#'*12,
    '#'*5 + '..,......' + '#'*8 + '%...' + '#'*12,
    '#'*4 + '..,..1...........' + '.....' + '#'*14,
    '#'*3 + '..,......' + '#'*28,
    '##.]' + ',....' + '#'*31,
    '##X' + '#'*37,
    '#'*40, '#'*40, '#'*40,
], {
    'L': E('Entrance', 'to_crypt', 'crypt@@', label='Врата Королевского склепа'), '(': E('Arrive', 'from_crypt'),
    'X': E('Exit', 'to_road', 'road@}'), ']': E('Arrive', 'from_road'),
    'S': E('Shrine', 'shrine_pass', 'crypt@@', label='Святилище перевала'),
    '$': E('Chest', 'guard_cache', 'gold:120;guard_amulet:1', 'item:guard_key', 'Окованный сундук старого стража', 'Сундук заперт. Ключ старого стража спрятан где-то в долине.'),
    'O': E('Ore', 'ore', 'ore:2', label='Рудная жила'),
    '%': E('Herb', 'herb', label='Горная трава'),
    '1': S('Wolf', 0), '2': S('Bandit', 0), '3': S('Ghost', 0), '4': S('Skeleton', 0),
})

# ---------------------------------------------------------------- Royal crypt (dungeon 3)
zone('crypt', 'Королевский склеп', 12, False, 'dungeon', 'pass', [
    '#'*34,
    '#'*13 + 'l...Y...l' + '#'*12,
    '#'*13 + '.P..}..P.' + '#'*12,
    '#'*13 + '.........' + '#'*12,
    '#'*17 + 'R' + '#'*16,
    '#'*12 + 'l.........l' + '#'*11,
    '#'*12 + '.I...J...Z.' + '#'*11,
    '#'*12 + '.....4.....' + '#'*11,
    '#'*12 + 'g.........g' + '#'*11,
    '#'*16 + '...' + '#'*15,
    '###.........####...####.........##',
    '###.g.g.g.g.####.^.####.g.g.g.g.##',
    '###...1.....####...####.....2...##',
    '###.g.g.g.g.####...####.g.g!g.g.##',
    '###..3....Q.####.^.####...2...$.##',
    '###.........####...####.........##',
    '#####.' + '#'*11 + '+' + '#'*11 + '.####',
    '###....P' + '.'*5 + 'P' + '.'*9 + 'P' + '.'*4 + 'P..###',
    '###' + '.'*6 + '1' + '.'*11 + '1' + '.'*8 + '###',
    '###l' + '.'*12 + '^' + '.'*12 + 'l###',
    '###..P' + '.'*6 + '.....@....' + '.'*5 + 'P..###',
    '#'*17 + 'D' + '#'*16,
    '#'*34, '#'*34,
], {
    'D': E('Exit', 'to_pass', 'pass@('), '@': E('Arrive', 'entry'),
    'Y': E('Exit', 'to_throne', 'throne@@', label='Тронный склеп'), '}': E('Arrive', 'from_throne'),
    '+': E('LockDoor', 'crypt_door', cond='item:crypt_key', label='Запертая дверь', msg='Дверь заперта. Ключ должен быть где-то в крыльях склепа.'),
    'R': E('RuneDoor', 'rune_door', cond='flag:runes_solved', label='Рунная дверь', msg='Каменная дверь с тремя гнёздами. Коснитесь рун в верном порядке.'),
    'I': E('Rune', 'rune_sun', '0', label='Руна Солнца'),
    'J': E('Rune', 'rune_moon', '1', label='Руна Луны'),
    'Z': E('Rune', 'rune_star', '2', label='Руна Звезды'),
    'Q': E('Chest', 'crypt_key_chest', 'crypt_key:1;bone:1', label='Саркофаг ключника'),
    '$': E('Chest', 'crypt_treasure', 'gold:90;greater_potion:1;bone:3', label='Сундук казначея'),
    '!': E('Note', 'rune_note', label='Надпись на плите'),
    '1': S('Skeleton', 0), '2': S('SkeletonArcher', 0), '3': S('Ghost', 0), '4': S('KnightGuard', 0),
})

# ---------------------------------------------------------------- Throne crypt: the original painted room
zone('throne', 'Тронный склеп', 13, False, 'boss', 'pass', None, {
    '@': E('Arrive', 'entry'),
    'D': E('Exit', 'to_crypt', 'crypt@}'),
    '9': S('FallenKnight', 9),
})

# ---------------------------------------------------------------- Castle Valden courtyard
zone('castle', 'Замок Вальден', 5, True, 'castle', 'castle', [
    '#'*34,
    'KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK',
    'KKKKKKKKKKKKKK..L...KKKKKKKKKKKKKK',
    'KKKKKKKKKKKKKK..(...KKKKKKKKKKKKKK',
    'KK....KKKKKKKK......KKKKKKKK....KK',
    'KK.!..KKKKKKKKl....lKKKKKKKK..$.KK',
    'KK.................................'[:32] + 'KK',
    'KK..B.....3...........3......B..KK',
    'KK............................. .KK'.replace(' ', '.'),
    'KKKKKK...P......P......P...KKKKKK',
    'K'*6 + '....................' + '...' + 'K'*5,
    'KK..........1.......1.........gKK',
    'KK..g.........................gKK',
    'KK..g.....4.......S.......4...gKK',
    'KK..g..........................KK',
    'KK....B.................B.....KKK',
    'KKKKKKKKKKKK.........KKKKKKKKKKKK',
    'KKKKKKKKKKKKK..,,,..KKKKKKKKKKKKK',
    '#'*14 + ',,)' + '#'*17,
    '#'*14 + '.D.' + '#'*17,
    '#'*34, '#'*34, '#'*34, '#'*34,
], {
    'L': E('Entrance', 'to_heart', 'heart@@', label='Лестница в Сердце под замком'), '(': E('Arrive', 'from_heart'),
    'D': E('Exit', 'to_village', 'village@)'), ')': E('Arrive', 'from_village'),
    'S': E('Shrine', 'shrine_castle', 'heart@@', label='Святилище замка'),
    '!': E('Note', 'mage_diary', label='Дневник придворного мага'),
    '$': E('Chest', 'castle_armory', 'gold:110;greater_potion:2;bone:2', label='Оружейный сундук'),
    '1': S('Skeleton', 0), '3': S('Ghost', 0), '4': S('KnightGuard', 0),
})

# ---------------------------------------------------------------- Heart beneath the castle (final)
zone('heart', 'Сердце под замком', 14, False, 'boss', 'castle', [
    '#'*30,
    '#'*7 + 'kk..........kk' + '#'*9,
    '#'*6 + 'k....5......5...k' + '#'*7,
    '#'*5 + 'l................l' + '#'*6,
    '#'*5 + '.........9........' + '#'*6,
    '#'*5 + 'k.................' + '#'*6,
    '#'*6 + '.......5.........' + '#'*7,
    '#'*7 + 'k..............' + '#'*8,
    '#'*10 + '.........' + '#'*11,
    '#'*12 + '.....' + '#'*13,
    '#'*7 + 'kk...' + '.....' + '...kk' + '#'*8,
    '#'*6 + '...1...' + '...' + '...3...' + '#'*7,
    '#'*6 + 'l.......' + '.' + '........l' + '#'*6,
    '#'*7 + '.....4.....1....' + '#'*7,
    '#'*9 + '.............' + '#'*8,
    '#'*12 + '..@..' + '#'*13,
    '#'*14 + 'D' + '#'*15,
    '#'*30, '#'*30, '#'*30,
], {
    'D': E('Exit', 'to_castle', 'castle@('), '@': E('Arrive', 'entry'),
    '1': S('Skeleton', 0), '3': S('Ghost', 0), '4': S('KnightGuard', 0),
    '5': S('Pylon', 9), '9': S('BoneKeeper', 9),
})

THRONE = None  # filled by the C++ layout room

def validate(z):
    rows = z['rows']
    if rows is None:
        return
    w = len(rows[0])
    for i, r in enumerate(rows):
        assert len(r) == w, f"{z['id']} row {i}: width {len(r)} != {w}: {r!r}"
    legend = z['legend']
    anchors = {}
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch in TERRAIN:
                continue
            assert ch in legend, f"{z['id']}: unknown char {ch!r} at {x},{y}"
            anchors.setdefault(ch, []).append((x, y))
    for ch in legend:
        assert ch in anchors, f"{z['id']}: legend {ch!r} unused"
    h = len(rows)

    def blocked(x, y):
        if not (0 <= x < w and 0 <= y < h):
            return True
        ch = rows[y][x]
        if ch in TERRAIN:
            return TERRAIN[ch] in ('Wall', 'Water', 'Prop', 'LowProp')
        d = legend[ch]
        if 'spawn' in d:
            return False
        return d['type'] in BLOCKING_ENTITIES and d['type'] not in ('Gate', 'LockDoor', 'RuneDoor')

    starts = [p for ch, ps in anchors.items() if 'type' in legend[ch] and legend[ch]['type'] == 'Arrive' for p in ps]
    sx, sy = starts[0]
    seen = {(sx, sy)}
    q = deque([(sx, sy)])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in seen and not blocked(*n):
                seen.add(n)
                q.append(n)
    for ch, ps in anchors.items():
        for (x, y) in ps:
            ok = (x, y) in seen or any((x + dx, y + dy) in seen for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            assert ok, f"{z['id']}: {ch!r} at {x},{y} unreachable"

def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def emit():
    out = ['// Generated by tools/zones.py — edit the Python source and regenerate.', '']
    for z in ZONES:
        validate(z)
        out.append('{' + f"{cstr(z['id'])},{cstr(z['name'])},{z['biome']},{'true' if z['surface'] else 'false'},{cstr(z['music'])},{cstr(z['region'])},")
        if z['rows'] is None:
            out.append(' {},')
        else:
            out.append(' {' + ','.join(cstr(r) for r in z['rows']) + '},')
        ents, spawns = [], []
        for ch, d in z['legend'].items():
            if 'spawn' in d:
                spawns.append('{' + f"'{ch}',combat::Kind::{d['spawn']},{d['group']}" + '}')
            else:
                c = ch if ch not in "'\\" else '\\' + ch
                ents.append('{' + f"'{c}',EntityType::{d['type']},{cstr(d['id'])},{cstr(d['arg'])},{cstr(d['cond'])},{cstr(d['label'])},{cstr(d['msg'])}" + '}')
        out.append(' {' + ','.join(ents) + '},')
        out.append(' {' + ','.join(spawns) + '}},')
    path = Path(__file__).resolve().parents[1] / 'src/zone_data.inc'
    path.write_text('\n'.join(out) + '\n', encoding='utf-8')
    print('zones:', len(ZONES), '->', path)

if __name__ == '__main__':
    emit()
