"""Author native 480x480 scenes for the confirmed clean glass design."""
from pathlib import Path
import json, math
ROOT=Path(__file__).resolve().parents[1]
BG,INK,TEAL,LIGHT,MUTED='#F3F6F8','#102632','#00AA98','#E9F7F4','#70818B'
objects=[]
backdrops={}
charset=''.join(chr(c) for c in range(32,127))+'\n'
def add(kind,parent,x,y,w,h,**kw):
    objects.append(dict(type=kind,parent=parent,x=x,y=y,w=w,h=h,**kw));return len(objects)-1
def box(p,x,y,w,h,color='#FFFFFF',**kw):
    return add('container',p,x,y,w,h,bg_color=color,radius=20,**kw)
def background(p,name):
    backdrops[p]='wall_'+name
    return add('image',p,0,0,480,480,name=backdrops[p],image='assets/home-wall.png' if name=='home' else 'assets/glass-wall.svg')
def glass(p,x,y,w,h):
    box(p,x,y+4,w,h,'#DCE5EB')
    g=add('glass',p,x,y,w,h,backdrop=backdrops[p],blur=22,tint_color='#F4FAFF',tint_opacity=92,radius=20)
    box(p,x,y,w,h,'#FFFFFF00',border_color='#FFFFFFCC',border_width=1)
    return g
def label(p,x,y,w,h,text,size=22,color=INK,name=None,center=False):
    kw=dict(text=text,font_size=size,fg_color=color)
    if center:kw['text_align']='center'
    if name:kw.update(name=name,bind=name,font_charset=charset)
    return add('label',p,x,y,w,h,**kw)
def button(p,x,y,w,h,text,action,filled=True,size=22):
    b=add('button',p,x,y,w,h,radius=18,bg_color=TEAL if filled else '#FFFFFF68',
          border_color='#FFFFFF',border_width=1,name=action,callback=action)
    label(b,0,0,w,h,text,size,'#FFFFFF' if filled else INK,center=True);return b
def icon(p,x,y,w,h,name):
    asset=f'generated-{name}.png'
    return add('image',p,x,y,w,h,image='assets/'+(asset if (ROOT/'ui/assets'/asset).exists() else name+'.svg'))
def group(name):
    p=box(-1,0,0,480,480,BG,name=name,bind=name,bind_target='visible',hidden=name!='home')
    background(p,name);return p
def header(p,title):
    b=button(p,8,8,88,60,'','back_'+str(len(objects)),False)
    icon(b,27,16,28,28,'back');label(p,115,18,250,42,title,25,center=True)
# Small code-authored vector assets, compiled into the native bundle.
paths={
 'camera':'<path d="M5 16H16L20 9H44L48 16H59V52H5Z"/><circle cx="32" cy="34" r="12"/>',
 'chat':'<path d="M9 9H55V45H30L17 56V45H9Z"/><circle cx="21" cy="27" r="1"/><circle cx="32" cy="27" r="1"/><circle cx="43" cy="27" r="1"/>',
 'mic':'<rect x="22" y="5" width="20" height="34" rx="10"/><path d="M13 29V33A19 19 0 0 0 51 33V29M32 52V60M23 60H41"/>',
 'wifi':'<path d="M6 24Q32 2 58 24M15 34Q32 19 49 34M24 44Q32 36 40 44"/><circle cx="32" cy="52" r="2"/>',
 'chip':'<rect x="14" y="14" width="36" height="36" rx="4"/><path d="M24 23H40V41H24ZM22 6V14M32 6V14M42 6V14M22 50V58M32 50V58M42 50V58M6 22H14M6 32H14M6 42H14M50 22H58M50 32H58M50 42H58"/>',
 'back':'<path d="M42 7L17 32L42 57"/>',
 'sun':'<circle cx="32" cy="32" r="12"/><path d="M32 3V10M32 54V61M3 32H10M54 32H61M11 11L16 16M48 48L53 53M11 53L16 48M48 16L53 11"/>',
 'speaker':'<path d="M6 24H17L31 12V52L17 40H6ZM41 22Q52 32 41 42M49 13Q67 32 49 51"/>',
 'rotate':'<path d="M52 23A23 23 0 1 0 53 43M52 7V23H36"/>',
 'rotation_lock':'<path d="M53 23A23 23 0 1 0 53 43M53 7V23H37"/><rect x="23" y="28" width="18" height="16" rx="3"/><path d="M27 28V23A5 5 0 0 1 37 23V28"/>',
 'gear':'<path d="M25 7H39L41 15L48 19L56 17L59 29L56 35V43L49 50L41 49L37 57H23L21 49L14 45L6 47L4 35L8 29V21L15 14L23 15Z"/><circle cx="32" cy="32" r="10"/>',
 'up':'<path d="M10 42L32 20L54 42"/>',
 'down':'<path d="M10 22L32 44L54 22"/>',
}
for name,path in paths.items():
    color=TEAL if name in ('camera','chat','mic') else INK
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><g fill="none" stroke="{color}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">{path}</g></svg>'
    (ROOT/f'ui/assets/{name}.svg').write_text(svg)
(ROOT/'ui/assets/glass-wall.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" width="480" height="480" viewBox="0 0 480 480"><rect width="480" height="480" fill="#F0F4F7"/><path d="M0 440C75 220 180 185 480 310L480 480H0Z" fill="#CFDFE9"/><path d="M0 480C180 230 250 240 480 395V480H0Z" fill="#BBDDD6" opacity="0.55"/><path d="M2 399C100 230 260 215 478 331" fill="none" stroke="#FFFFFF" stroke-width="3" opacity="0.8"/></svg>''')
(ROOT/'ui/assets/home-wall.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" width="480" height="480" viewBox="0 0 480 480"><defs><linearGradient id="sky" x2="1" y2="1"><stop stop-color="#E7F7FE"/><stop offset="0.5" stop-color="#F4FCFD"/><stop offset="1" stop-color="#DEEFFB"/></linearGradient><radialGradient id="orb"><stop stop-color="#FFFFFF"/><stop offset="0.35" stop-color="#CDFBEA"/><stop offset="1" stop-color="#70DAD5"/></radialGradient></defs><rect width="480" height="480" fill="url(#sky)"/><path d="M2 165C85 32 285 16 421 130C490 189 475 334 372 403C238 493 69 431 4 347" fill="none" stroke="#A8E5E2" stroke-width="1"/><path d="M1 182C73 67 306 7 448 182C514 263 414 439 198 449" fill="none" stroke="#FFFFFF" stroke-width="2"/><path d="M5 140C102 80 273 27 432 98M15 379C72 429 215 468 368 423" fill="none" stroke="#B0E8F5" stroke-width="1"/><path d="M13 197C94 70 152 8 177 39C198 82 45 123 18 124" fill="none" stroke="#9EDDDD" stroke-width="1"/><g fill="url(#orb)"><circle cx="56" cy="62" r="12"/><circle cx="154" cy="44" r="6"/><circle cx="342" cy="77" r="7"/><circle cx="452" cy="190" r="6"/><circle cx="77" cy="429" r="13"/><circle cx="359" cy="434" r="6"/><circle cx="30" cy="168" r="5"/></g><g fill="#BAEEF3" stroke="#FFFFFF" stroke-width="1"><path d="M29 106L42 95L56 106L43 124ZM29 106L43 113L43 124M43 113L56 106M42 95L43 113"/><path d="M417 428L432 417L449 429L434 450ZM417 428L434 436L434 450M434 436L449 429M432 417L434 436"/></g><g fill="#54D8CA"><path d="M92 161L95 166L101 167L96 171L95 177L91 172L85 171L90 167Z"/><path d="M385 94L387 98L392 99L388 102L387 107L384 103L379 102L383 99Z"/></g></svg>''')
p=group('home')
# Separate status dots bind to live network and verified API state.
b=box(p,306,20,70,44,'#FFFFFF70')
icon(b,9,8,30,28,'wifi');add('container',b,48,18,9,9,radius=5,bg_color='#A4AFB8',name='wifi_dot',bind='wifi_dot',bind_target='color')
b=box(p,386,20,70,44,'#FFFFFF70')
icon(b,9,8,28,28,'chip');add('container',b,48,18,9,9,radius=5,bg_color='#A4AFB8',name='api_dot',bind='api_dot',bind_target='color')
label(p,24,102,432,62,'今天想学什么？',36,center=True)
for action,x,text,img in [('photo',24,'拍题讲解','camera'),('ask',248,'问个问题','chat')]:
    glass(p,x,205,208,196)
    b=button(p,x,205,208,196,'',action,False);objects[b]['bg_color']='#FFFFFF08'
    icon(b,38,14,132,132,img);label(b,0,140,208,48,text,28,center=True)
label(p,24,422,432,26,'顶部下拉可调整亮度与声音',16,MUTED,center=True)
p=group('camera');header(p,'拍题')
glass(p,24,76,432,240)
label(p,36,145,408,60,'相机正在准备',26,MUTED,'camera_hint',True)
add('image',p,32,84,416,224,image='assets/camera.png',name='preview',bind='preview')
label(p,24,325,432,30,'拍清题号和完整题干，稍后选题',18,MUTED,center=True)
tools=add('container',p,0,0,480,480,name='camera_tools',bind='camera_tools',bind_target='visible')
b=button(tools,178,354,124,104,'','capture',False);objects[b].update(bg_color='#FFFFFF00',border_width=0);objects[b].pop('border_color')
icon(b,10,0,104,104,'shutter')
b=button(tools,42,368,88,72,'','light',False);icon(b,14,6,60,60,'light')
label(tools,35,440,102,25,'补光',16,INK,'light_label',True)
b=button(tools,350,368,88,72,'','flip_camera',False);icon(b,14,6,60,60,'flip180')
label(tools,348,440,110,25,'',16,TEAL,'flip_label',True)
confirm=box(p,0,362,480,84,BG,name='confirm',bind='confirm',bind_target='visible',hidden=True)
button(confirm,24,12,170,58,'重拍','retake',False,24);button(confirm,208,12,248,58,'识别题目','send',True,24)
p=group('questions');header(p,'选择要讲的题')
label(p,24,82,432,35,'识别到 0 道题',24,INK,'question_count')
label(p,24,121,432,27,'核对题号和题干，点击后开始讲解',18,MUTED,'question_instruction')
for i in range(3):
    y=159+i*76
    g=add('container',p,24,y,432,67,name=f'question_card_{i}',bind=f'question_card_{i}',bind_target='visible')
    b=button(g,0,0,432,67,'',f'pick_{i}',False)
    label(b,16,3,260,30,'',21,TEAL,f'question_number_{i}')
    label(b,285,7,108,24,'',15,MUTED,f'question_location_{i}')
    label(b,16,34,380,29,'',18,INK,f'question_summary_{i}')
    label(b,397,17,26,36,'›',28,TEAL,center=True)
b=button(p,24,398,130,44,'‹ 上一页','question_previous',False,19)
objects[b].update(enabled=True,disabled_color='#C7D0D6',disabled_opacity=140)
label(p,165,398,150,44,'1 / 1',20,INK,'question_page',True)
b=button(p,326,398,130,44,'下一页 ›','question_next',False,19)
objects[b].update(enabled=True,disabled_color='#C7D0D6',disabled_opacity=140)
button(p,116,443,248,36,'没找到？重新拍照','question_retake',False,17)
p=group('reading');header(p,'题目讲解')
objects[-1].update(x=105,w=145)
b=button(p,260,20,94,38,'换一题','choose_again',False,16)
objects[b].update(bind='choose_again',bind_target='visible')
button(p,365,20,98,38,'换个讲法','explain',False,16)
glass(p,24,76,432,234)
for i in range(6):label(p,38,87+i*35,408,34,'',22,INK,f'line_{i}')
for action,x,text in [('previous',24,'‹ 上一页'),('next',326,'下一页 ›')]:
    b=button(p,x,324,130,46,text,action,False,20)
    objects[b].update(enabled=True,disabled_color='#C7D0D6',disabled_opacity=140)
label(p,165,324,150,46,'1 / 1',22,INK,'page_hint',True)
b=button(p,24,386,204,58,'','read',False,23);icon(b,18,17,28,28,'speaker')
label(b,52,0,146,58,'朗读',23,INK,'read_label',True)
button(p,240,386,216,58,'继续提问','followup',True,23)
p=group('voice');header(p,'语音提问')
wave=button(p,40,82,400,280,'','voice_wave',False);objects[wave].update(callback='voice_toggle',bg_color='#FFFFFF00',border_width=0);objects[wave].pop('border_color')
icon(wave,125,0,150,150,'mic')
for frame in range(9):
    name=f'wave_{frame}'
    add('image',wave,0,160,400,100,name=name,bind=name,bind_target='visible',hidden=frame!=0,image=f'assets/wave-{frame}.svg')
button(p,24,399,170,50,'取消','cancel_voice',False,22)
b=button(p,208,399,248,50,'开始说话','voice_record',True,22);objects[b]['callback']='voice_toggle'
objects[b+1].update(name='voice_record_label',bind='voice_record_label',font_charset=charset)
p=group('voice_review');header(p,'识别确认')
label(p,24,66,432,30,'确认文字后，再发送',19,MUTED,'review_instruction',True)
glass(p,24,105,432,216)
for i in range(6):label(p,38,110+i*34,408,33,'',22,INK,f'review_line_{i}')
for action,x,text in [('review_previous',24,'‹ 上一页'),('review_next',326,'下一页 ›')]:
    b=button(p,x,333,130,42,text,action,False,19)
    objects[b].update(callback='previous' if x==24 else 'next',enabled=True,disabled_color='#C7D0D6',disabled_opacity=140)
label(p,165,333,150,42,'1 / 1',21,INK,'review_page',True)
button(p,24,393,170,54,'重新说','voice_retake',False,22)
button(p,208,393,248,54,'确认发送','send_text',True,23)
p=group('waiting');header(p,'正在思考')
label(p,24,109,432,42,'正在处理你的问题',27,center=True)
glass(p,24,181,432,145);label(p,40,190,400,28,'识别文字 / 处理进度',18,MUTED)
label(p,40,232,400,86,'请稍候…',21,INK,'waiting_text')
button(p,110,390,260,54,'取消','cancel_waiting',False)
p=group('settings_panel');header(p,'连接与设置')
glass(p,24,80,432,98)
label(p,38,90,405,34,'Wi-Fi：未连接',22,INK,'network')
label(p,38,130,405,34,'API：尚未配置',22,INK,'model_status')
button(p,24,272,432,58,'用手机配置 Wi-Fi 和 Token','setup',True,22)
for i in range(3):label(p,32,352+i*28,420,28,'',18,MUTED,f'setup_{chr(97+i)}')
b=button(p,24,190,432,52,'','grade_toggle',False)
label(b,18,0,100,52,'年级',22)
label(b,150,0,220,52,'初一',22,INK,'grade_label',True)
icon(b,388,18,20,18,'down')
menu=add('container',p,24,246,432,216,name='grade_menu',bind='grade_menu',bind_target='visible',hidden=True,bg_color='#F4FCFD',radius=18)
for i,title in enumerate(['一年级','二年级','三年级','四年级']):
    b=button(menu,4,i*48,424,48,'',f'grade_pick_{i}',False)
    objects[b].update(bind=f'grade_color_{i}',bind_target='color',border_width=0)
    objects[b].pop('border_color')
    label(b,18,0,388,48,title,23,INK,f'grade_item_{i}')
    objects[-1]['font_charset']=charset+'一年级二三四五六初高'
label(menu,12,192,408,24,'上下滑动选择年级',15,MUTED,center=True)
p=group('error_panel');header(p,'暂时没成功')
label(p,24,110,432,50,'我们再试一次',30,center=True)
label(p,40,184,400,90,'',22,INK,'error_text',True)
button(p,24,340,432,58,'重试','retry',True)
label(-1,24,453,432,24,'',15,MUTED,'status')
add('shape',-1,214,5,52,4,shape='rect',bg_color='#B4C7D0',radius=2)
p=add('container',-1,0,0,480,480,name='quick_settings',bind='quick_settings',bind_target='visible',hidden=True,bg_color=BG)
background(p,'quick_settings')
add('shape',p,210,12,60,5,shape='rect',bg_color='#B4C7D0',radius=3)
label(p,120,38,240,40,'控制面板',26,center=True)
close=button(p,386,35,70,40,'收起','quick_close',False,18)
glass(p,24,94,432,342)
for name,y,minimum,value,img,title in [('brightness',126,10,80,'sun','亮度'),('volume',220,0,50,'speaker','音量')]:
    icon(p,40,y-28,30,30,img);label(p,84,y-34,180,30,title,21)
    label(p,350,y-34,85,30,str(value)+'%',20,MUTED,name+'_percent',True)
    add('slider',p,24,y,432,44,name='quick_'+name,min=minimum,max=100,value=value,track_size=12,bg_color='#DFE8EE',fg_color=TEAL,knob_color='#FFFFFF',radius=8,events=[dict(event='value',action='call',target_name='quick_'+name)])
label(p,40,282,260,34,'自动朗读',22)
for on in [False,True]:
    name='speech_on' if on else 'speech_off'
    g=add('container',p,380,282,64,36,name=name,bind=name,bind_target='visible',hidden=not on)
    b=add('button',g,0,0,64,36,name=name+'_button',callback='quick_speech',bg_color=TEAL if on else '#DAE3E9',radius=18)
    add('container',b,32 if on else 4,4,28,28,bg_color='#FFFFFF',radius=14)
add('shape',p,40,331,400,1,shape='rect',bg_color='#DFE6EC')
for automatic in [False,True]:
    name='direction_auto' if automatic else 'direction_locked'
    g=add('container',p,40,348,184,72,name=name,bind=name,bind_target='visible',hidden=not automatic)
    b=button(g,0,0,184,72,'',name+'_button',False);objects[b]['callback']='direction'
    icon(b,69,13,46,46,'rotation_lock')
    if not automatic:objects[b]['bg_color']='#A9E8DF'
b=button(p,256,348,184,72,'','settings',False);icon(b,69,13,46,46,'gear')
# The application closes the lightweight overlay before entering settings.
label(p,24,459,432,21,'向上滑动收起',16,MUTED,center=True)
scene=dict(screen='learn',w=480,h=480,screen_bg=BG,font='fonts/LearningSans.ttf',default_font_size=22,swipe=False,objects=objects)
(ROOT/'ui/main.json').write_text(json.dumps(scene,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'Generated {len(objects)} native objects')
