const products=[
{id:1,n:'Cherry Shrimp',c:'shrimp',p:4.99,e:'🦐',d:'Hardy red freshwater shrimp for planted aquariums.'},
{id:2,n:'Blue Dream Shrimp',c:'shrimp',p:6.49,e:'🦐',d:'Deep blue Neocaridina with loads of personality.'},
{id:3,n:'Shrimp Mineral Bites',c:'food',p:8.99,e:'🟢',d:'Mineral-rich sinking food for healthy molts.'},
{id:4,n:'Nano Fish Pellets',c:'food',p:7.49,e:'🟤',d:'Tiny slow-sinking pellets for community fish.'},
{id:5,n:'Algae Wafers',c:'food',p:6.99,e:'🟩',d:'Plant-based wafers for bottom feeders.'},
{id:6,n:'Moss Hide',c:'supplies',p:11.99,e:'🌿',d:'A cozy grazing and hiding spot for shrimp.'},
{id:7,n:'Mini Sponge Filter',c:'supplies',p:13.99,e:'🧽',d:'Gentle filtration designed for nano tanks.'},
{id:8,n:'Shrimp Mineral Stone',c:'supplies',p:9.49,e:'🪨',d:'Slow-release mineral support for shrimp tanks.'}];
let cart=JSON.parse(localStorage.getItem('shrimply-cart')||'[]');const $=s=>document.querySelector(s),money=n=>'$'+n.toFixed(2);
function save(){localStorage.setItem('shrimply-cart',JSON.stringify(cart));renderCart()}
function renderProducts(){let q=$('#search').value.toLowerCase(),c=$('#cat').value;let a=products.filter(x=>(c==='all'||x.c===c)&&(x.n+' '+x.d).toLowerCase().includes(q));$('#products').innerHTML=a.map(x=>`<article class="card"><div class="pic">${x.e}</div><h3>${x.n}</h3><p>${x.d}</p><div class="row"><b>${money(x.p)}</b><button class="add" onclick="add(${x.id})">+ Add</button></div></article>`).join('')||'<div class="empty">No tiny treasures found.</div>'}
function add(id){let x=cart.find(x=>x.id===id);x?x.q++:cart.push({id,q:1});save();openCart()}
function change(id,d){let x=cart.find(x=>x.id===id);if(!x)return;x.q+=d;if(x.q<1)cart=cart.filter(x=>x.id!==id);save()}
function renderCart(){let count=cart.reduce((a,x)=>a+x.q,0),total=0;$('#count').textContent=count;$('#items').innerHTML=cart.length?cart.map(x=>{let p=products.find(p=>p.id===x.id);total+=p.p*x.q;return `<div class="line"><div class="mini">${p.e}</div><div><b>${p.n}</b><br><span>${money(p.p)} × ${x.q}</span><br><button onclick="change(${p.id},-1)">−</button> ${x.q} <button onclick="change(${p.id},1)">+</button></div><b>${money(p.p*x.q)}</b></div>`}).join(''):'<div class="empty">Your cart is empty.<br>It needs shrimp.</div>';$('#total').textContent=money(total)}
function openCart(){$('#cart').classList.add('open');$('#shade').classList.add('show')}function closeCart(){$('#cart').classList.remove('open');$('#shade').classList.remove('show')}
$('#search').oninput=renderProducts;$('#cat').onchange=renderProducts;$('#openCart').onclick=openCart;$('#closeCart').onclick=closeCart;$('#shade').onclick=closeCart;$('#clear').onclick=()=>{cart=[];save()};
$('#checkout').onclick=()=>{if(!cart.length)return;let s=cart.map(x=>{let p=products.find(p=>p.id===x.id);return `${x.q} × ${p.n} — ${money(p.p*x.q)}`}).join('<br>');$('#summary').innerHTML='<b>Order</b><br>'+s+'<hr>Total: '+$('#total').textContent;$('#modal').classList.add('show');closeCart()};$('#closeModal').onclick=()=>$('#modal').classList.remove('show');
$('#form').onsubmit=e=>{e.preventDefault();let f=new FormData(e.target),items=cart.map(x=>{let p=products.find(p=>p.id===x.id);return `${x.q} x ${p.n}`}).join(', ');let body=encodeURIComponent(`New Shrimply order\nName: ${f.get('name')}\nEmail: ${f.get('email')}\nNotes: ${f.get('notes')}\nItems: ${items}\nTotal: ${$('#total').textContent}`);location.href='mailto:?subject=Shrimply%20order&body='+body};renderProducts();renderCart();