#!/usr/bin/env python3
"""Analyze a completed run; PNG-domain SSIM is separate from native linear RMSE."""
import argparse,csv,math
from pathlib import Path
import numpy as np
from PIL import Image
from skimage.metrics import structural_similarity

def main():
    ap=argparse.ArgumentParser();ap.add_argument('run',type=Path);a=ap.parse_args();r=a.run
    if not (r/'status.txt').exists() or (r/'status.txt').read_text().splitlines()[0]!='complete':raise SystemExit('Run not marked complete')
    timing={(x['case'],x['method']):x for x in csv.DictReader((r/'timing_summary.csv').open())}
    quality=list(csv.DictReader((r/'quality.csv').open()))
    fields=['case','method','median_ms','speedup_vs_m0','speedup_vs_m1','linear_rmse','display_psnr_db','png_ssim','reference_psnr_db','nonfinite_values']
    with (r/'analysis.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for q in quality:
            case,m=q['case'],q['method'];t=timing[case,m];med=float(t['median_ms']);refpath=r/'captures'/f'{case}-ref.png';p=r/'captures'/f'{case}-{m}.png'
            with Image.open(refpath) as im:ref=np.asarray(im.convert('RGB'))
            with Image.open(p) as im:img=np.asarray(im.convert('RGB'))
            ssim=structural_similarity(ref,img,data_range=255,channel_axis=-1)
            refq=next((x for x in quality if x['case']==case and x['method']=='ref2'),None)
            row=dict(case=case,method=m,median_ms=med,speedup_vs_m0=float(timing[case,'m0']['median_ms'])/med if med else math.nan,speedup_vs_m1=float(timing[case,'m1']['median_ms'])/med if med else math.nan,linear_rmse=q['linear_rmse'],display_psnr_db=q['display_psnr_db'],png_ssim=ssim,reference_psnr_db=refq['display_psnr_db'] if refq else '',nonfinite_values=q['nonfinite_values']);w.writerow(row)
            if m not in ('ref','ref2'):
                err=np.max(np.abs(img.astype(np.float32)-ref.astype(np.float32)),axis=2);err=np.uint8(np.clip(err*4,0,255));out=r/'errors';out.mkdir(exist_ok=True);Image.fromarray(err).save(out/f'{case}-{m}.png')
    print(r/'analysis.csv')
if __name__=='__main__':main()
