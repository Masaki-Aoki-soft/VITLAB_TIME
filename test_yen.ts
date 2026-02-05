
import { runYen } from './src/lib/exec-utils';

async function test() {
    console.log('Testing runYen...');
    try {
        // args: startNode, endNode, walkingSpeed, kGradient
        const result = await runYen(22, 25, 80, 0.5);
        console.log('Result:', result);
    } catch (e: any) {
        console.error('Error:', e.message);
    }
}

test();
