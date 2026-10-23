"""Merge licensed Chinese/Latin outlines for dynamic Qwen response glyphs."""
from pathlib import Path
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.merge import Merger
from fontTools.ttLib.scaleUpem import scale_upem
import tempfile

ROOT=Path(__file__).resolve().parents[1]
def main():
    fonts=ROOT/'ui/fonts'
    # Keep every glyph available in the provided SC font, add missing maths
    # from the template's DejaVu font. The existing font licenses are retained.
    chinese=TTFont(fonts/'NotoSansSC-Regular.ttf')
    available=set(chinese.getBestCmap())
    latin=TTFont(fonts/'DejaVuSans.ttf')
    extra=(set(range(32,127))|set(map(ord,'★×÷−√π°²³△∠∥⊥≈≠≤≥→“”‘’—')))-available
    options=subset.Options();options.layout_features=[]
    options.drop_tables += ['MATH']
    sub=subset.Subsetter(options=options);sub.populate(unicodes=extra);sub.subset(latin)
    scale_upem(latin,chinese['head'].unitsPerEm)
    for font in (chinese,latin):
        for tag in ('GDEF','GSUB','GPOS','MATH','BASE','vhea','vmtx'):
            if tag in font:del font[tag]
    with tempfile.TemporaryDirectory() as directory:
        patch=Path(directory)/'extra.ttf';latin.save(patch)
        base=Path(directory)/'base.ttf';chinese.save(base)
        merged=Merger().merge([str(base),str(patch)])
        merged.save(fonts/'LearningSans.ttf')
    print('Dynamic font:',fonts/'LearningSans.ttf')
if __name__=='__main__':main()
