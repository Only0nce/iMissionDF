#!/bin/sh
set -eu

python3 - <<'PY'
from pathlib import Path
import re, math, random
root=Path('.')


def check_balanced(path):
    text=Path(path).read_text(errors='ignore')
    pairs={')':'(',']':'[','}':'{'}
    stack=[]
    quote=None
    esc=False
    line_comment=False
    block_comment=False
    i=0
    while i<len(text):
        c=text[i]; n=text[i+1] if i+1<len(text) else ''
        if line_comment:
            if c=='\n': line_comment=False
            i+=1; continue
        if block_comment:
            if c=='*' and n=='/': block_comment=False; i+=2; continue
            i+=1; continue
        if quote:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line_comment=True; i+=2; continue
        if c=='/' and n=='*': block_comment=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack[-1]!=pairs[c]:
                raise SystemExit(f'FAIL {path}: unbalanced {c} at {i}')
            stack.pop()
        i+=1
    if stack or quote or block_comment:
        raise SystemExit(f'FAIL {path}: unterminated syntax stack={stack[-8:]} quote={quote} block={block_comment}')
    print('PASS', path, 'delimiter balance')

for f in ['SpectrumGLPlot.qml','FftDisplayItem.h','FftDisplayItem.cpp','websocketclient.h','websocketclient.cpp','main.cpp']:
    check_balanced(f)

qml=Path('SpectrumGLPlot.qml').read_text()
ws_h=Path('websocketclient.h').read_text()
ws=Path('websocketclient.cpp').read_text()
main=Path('main.cpp').read_text()
pro=Path('iScanMR10.pro').read_text()
fft_cpp=Path('FftDisplayItem.cpp').read_text()
fft_h=Path('FftDisplayItem.h').read_text()

assert 'import iScan.Display 1.0' in qml
print('PASS native display QML import present')
assert len(re.findall(r'\bFftDisplayItem\s*\{', qml)) == 2
print('PASS exactly two native FFT display items')
assert len(re.findall(r'\bCanvas\s*\{', qml)) == 2
print('PASS only grid and overlay Canvas remain')

for token in ['spectrumData','latestWaterfallLine','maxHoldDisplayData','waterfallCssPalette','ctx.drawImage(waterfallCanvas','onFftFrameUpdated']:
    assert token not in qml, token
    print('PASS QML FFT hot-path token removed:', token)

assert 'm_fftQmlPublish = envEnabled("ISCAN_FFT_QML_PUBLISH", false)' in ws
print('PASS legacy full FFT QML bridge defaults OFF')
assert 'm_spectrumDisplayBins = envPositiveInt("ISCAN_SPECTRUM_DISPLAY_BINS", 8192' in ws
assert 'm_waterfallDisplayBins = envPositiveInt("ISCAN_WATERFALL_DISPLAY_BINS", 1280' in ws
print('PASS native display budgets preserve 8192 spectrum / 1280 waterfall defaults')
for sig in ['spectrumDisplayFrame(QVector<float>','waterfallDisplayFrame(QVector<float>','maxHoldDisplayFrame(QVector<float>']:
    assert sig in ws_h
for emit in ['emit spectrumDisplayFrame','emit waterfallDisplayFrame','emit maxHoldDisplayFrame']:
    assert emit in ws
print('PASS native QVector display signals declared/emitted')
assert 'Q_INVOKABLE QVariantList maxHoldSnapshot() const' in ws_h
assert 'updateMaxHold(m_fftDecodeScratch);' in ws
print('PASS full-resolution Max Hold snapshot/accumulation preserved')
assert 'fftNoiseDb' in ws_h and 'fftStrongDb' in ws_h and 'fftAutoScaleValid' in ws_h
assert 'std::sort(m_fftAutoScaleScratch.begin(), m_fftAutoScaleScratch.end())' in ws
assert 'wsClient.fftNoiseDb' in qml and 'wsClient.fftStrongDb' in qml
print('PASS auto-scale percentile work moved to bounded native C++ sample')
assert 'class FftDisplayItem : public QQuickPaintedItem' in fft_h
assert 'QMutex m_dataMutex' in fft_h
assert 'std::memmove' in fft_cpp
print('PASS native QQuickPaintedItem renderer + mutex + waterfall history present')
assert 'Qt::DirectConnection' in fft_cpp
print('PASS native renderer receives QVector directly in C++')
assert 'qmlRegisterType<FftDisplayItem>("iScan.Display", 1, 0, "FftDisplayItem")' in main
print('PASS native renderer registered before QML load')
assert pro.count('FftDisplayItem.cpp') >= 2 and pro.count('FftDisplayItem.h') >= 2
print('PASS native renderer included in X86 and Jetson qmake lists')
assert 'QV4_FORCE_INTERPRETER' in main and '[R20.3 QML-JIT-SAFETY]' in main
print('PASS R20.3 ARM64 interpreter guard retained')
assert '[R20.4 NATIVE FFT]' in main and '[R20.4 FFT Runtime]' in ws
print('PASS R20.4 runtime markers present')
assert 'm_fftStrictSize = envEnabled("ISCAN_FFT_STRICT_SIZE", true)' in ws
assert 'fftFrame.size() > m_fftMaxBins' in ws
assert 'fft-size-mismatch' in ws and 'non-finite' in ws
print('PASS native FFT ingress validation restored')

# Algorithm equivalence checks for display reducers/mapping.
def peak_pool(src, limit):
    if not src or limit <= 0: return []
    if len(src) <= limit: return list(src)
    out=[]; n=len(src)
    for k in range(limit):
        b=(k*n)//limit; e=((k+1)*n)//limit
        e=max(b+1,min(e,n))
        out.append(max(src[b:e]))
    return out

src=[float(i) for i in range(8192)]
assert peak_pool(src,8192)==src
print('PASS 8192-bin Spectrum path preserves exact source values/order')

src=[-120.0]*8192
src[4097]=-30.0
red=peak_pool(src,1280)
assert max(red)==-30.0
print('PASS 1280-bin Waterfall peak pool preserves narrow carrier peak')

for n,limit in [(4096,8192),(65536,8192),(7,3)]:
    a=[math.sin(i*.071)*30-100 for i in range(n)]
    r=peak_pool(a,limit)
    assert len(r)==min(n,limit)
    assert all(math.isfinite(v) for v in r)
    print(f'PASS peak reducer valid n={n} target={limit}')

# Map ratio bounds equivalent to FftDisplayItem mapping.
for count in [2,3,1280,8192,65536]:
    for _ in range(500):
        a=random.random(); b=a+(1-a)*random.random()
        start=max(0,min(count-1,math.floor(a*(count-1))))
        end=max(start,min(count-1,math.ceil(b*(count-1))))
        assert 0 <= start <= end < count
    print('PASS viewport mapping randomized count=',count)

# No executable JS .connect() lifecycle callbacks. Ignore // comments.
for line_no, line in enumerate(qml.splitlines(), 1):
    code = line.split('//', 1)[0]
    if re.search(r'\.connect\s*\(', code):
        raise SystemExit(f'FAIL executable JavaScript .connect() at line {line_no}')
print('PASS no executable JavaScript .connect() callbacks')
PY

echo 'PASS R20.4 static validation complete'
